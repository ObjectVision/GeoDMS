// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcInterface.h"

//#include <boost/asio/io_service.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket/stream.hpp>

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ShvBase.h"

#include "WmsLayer.h"
#include "parallel/dms_task.h"
#include "DataView.h"
#include "GraphVisitor.h"
#include "GridDrawer.h"
#include "LayerClass.h"
#include "ViewPort.h"

#include "stg/AbstrStorageManager.h"

#include "OperationContext.h"
#include "DataArray.h"
#include "act/MainThread.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "geo/PointOrder.h"
#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "utl/encodes.h"

#include "gdal/gdal_base.h"
#include "gdal/gdal_grid.h"

#if defined(MG_DEBUG)
#define MG_DEBUG_WMS
#endif

#define WMS_TIMER_SECONDS 3
#define WMS_MAX_CONCURRRENT_REQUESTS 8

namespace wms {
	using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
	namespace http = boost::beast::http;  // from <boost/beast/http.hpp>
	namespace ssl = boost::asio::ssl;
	using ssl_socket = ssl::stream<tcp::socket>;

	CrdRect tile_matrix::WorldExtents() const
	{
		return CrdRect(m_Raster2World.Offset(), m_Raster2World.Apply(RasterSize()));
	}
	raster_pos tile_matrix::RasterPosition(tile_pos t) const
	{
		return raster_pos(t) * raster_pos(m_TileSize);
	}
	Range<raster_pos> tile_matrix::RasterExtents(tile_pos t) const
	{
		return Range<raster_pos>(RasterPosition(t), RasterPosition(t + tile_pos(1, 1)));
	}
	CrdPoint tile_matrix::WorldPosition(tile_pos t) const
	{
		return m_Raster2World.Apply(Convert<CrdPoint>(RasterPosition(t)));
	}

	CrdRect tile_matrix::WorldExtents(tile_pos t) const
	{
		return m_Raster2World.Apply(Convert<CrdRect>(RasterExtents(t)));
	}

	raster_pos tile_matrix::RasterSize() const
	{
		return RasterPosition(m_MatrixSize);
	}

	grid_coord_key tile_matrix::GridCoordKey() const
	{
		return grid_coord_key(m_Raster2World, IRect(IPoint(0, 0), RasterSize()));
	}

	enum class image_status { undefined, loading, ready };

	struct image_info {
		image_status m_Status = image_status::undefined;
		SharedStr m_FileName;
		~image_info() {
//			dms_assert(m_Status != image_status::loading);
		}
	};

	std::shared_ptr<boost::asio::io_context> GetIOC()
	{
		static std::weak_ptr<boost::asio::io_context> s_IOC;
		auto ioc = s_IOC.lock();
		if (!ioc)
		{
			ioc = std::make_shared < boost::asio::io_context>();
			s_IOC = ioc;
		}
		return ioc;
	}
	void ProcessPendingTasks();

	struct Host {
		Host(std::string hostName)
			:	m_Name(hostName)
			,	m_Resolver{ * m_IOC.get() }
		{
			m_SslContext.set_default_verify_paths();
			m_SslContext.set_verify_mode(ssl::verify_none);
//			m_SslContext.load_verify_file("ca.pem");

			// Look up the domain name
			try 
			{
				m_Results = m_Resolver.resolve(hostName.c_str(), "https");
			}
			catch (...)
			{
				auto result = catchException(false);
				reportF(MsgCategory::background_layer_connection, SeverityTypeID::ST_Error, "Unable to resolve host: %s", hostName); //result->GetAsText().c_str()
			}
		}

		template <typename Socket, typename Continuation>
		void async_connect(Socket& socket, Continuation&& continuation) const
		{
			boost::asio::async_connect(socket, m_Results.begin(), m_Results.end(), std::forward<Continuation>(continuation)); // Make the connection on the IP address we get from a lookup
		}

		CharPtr Name() const { return m_Name.c_str(); }

		std::string m_Name;
		std::shared_ptr<boost::asio::io_context> m_IOC = GetIOC();
		mutable ssl::context m_SslContext = ssl::context(boost::asio::ssl::context::sslv23);

		tcp::resolver m_Resolver;
		tcp::resolver::results_type m_Results;
	};


	// see also: https://stackoverflow.com/questions/11356742/boost-async-functions-and-shared-ptrs
	struct TileLoader : enable_shared_from_this_base<TileLoader>
	{
		static std::atomic<UInt32> s_InstanceCount;

		TileLoader(const WmsLayer* owner, const Host& host, WeakStr target, WeakStr fileName, tile_id key, image_format_type ift)
			: m_Owner(owner->shared_from_base<const WmsLayer>())
			, m_SslSocket( *host.m_IOC, host.m_SslContext )
			, m_Timer{ * host.m_IOC }
			, m_Target(target)
			, m_Request{ http::verb::get, m_Target.c_str(), 11 }
			, m_FileName(fileName)
			, m_Key(key)
			, m_ImageFormatType(ift)
		{
			m_SslSocket.set_verify_mode(ssl::verify_none);
			m_SslSocket.set_verify_callback(ssl::host_name_verification(host.m_Name));

			MG_DEBUGCODE(report(MsgCategory::background_layer_request, "ctor", false));
			++s_InstanceCount;
		}

		~TileLoader();
 
		void SetTimer()
		{
			MG_DEBUGCODE(report(MsgCategory::background_layer_request, "SetTimer", true));
			m_Timer.expires_from_now(boost::posix_time::seconds(WMS_TIMER_SECONDS));
			m_Timer.async_wait([self = shared_from_this()](boost::system::error_code ec)
			{
				MG_DEBUGCODE(self->report(MsgCategory::background_layer_request, ("TimerExpired: " + ec.message()).c_str(), true));
			});
		}
		void Run(const Host& host) // in separate method because shared_from_this can only be called after completion of make_shared<TileLoader>(...);
		{
			MG_DEBUGCODE(report(MsgCategory::background_layer_request, "Run", true));

			m_Request.set(http::field::host, host.Name());
			m_Request.set(http::field::connection, "keep-alive");
			m_Request.set(http::field::user_agent, "GeoDMS");

			// Look up the domain named
			report(MsgCategory::background_layer_connection, SeverityTypeID::ST_MajorTrace, ("connect to https://" + host.m_Name).c_str(), ", result: OK", true);
			host.async_connect(m_SslSocket.lowest_layer(), [self = shared_from_this()](boost::system::error_code ec, auto iter)
				{ 
					auto owner = self->m_Owner.lock();
					if (owner) self->on_connect(ec);  
				//	if (owner) self->on_handshake(ec);
				}
			);

			SetTimer();

			if (s_InstanceCount == 1)
				PostPendingTasks();
		}

		void PostPendingTasks()
		{
			auto wmsLayer = m_Owner.lock(); if (!wmsLayer) return;
			auto dv = wmsLayer->GetDataView().lock(); if (!dv) return;
			dv->PostGuiOper([]
				{ 
					static dms_task runTask;
					runTask = dms_task([] { ProcessPendingTasks(); });
				}
			);
		}

		void report(MsgCategory ct, SeverityTypeID st, CharPtr what, CharPtr msg, bool alive);
		bool report_status(const boost::system::error_code& ec, CharPtr what);
		void report(MsgCategory ct, CharPtr what, bool alive)
		{
			report(MsgCategory::background_layer_request, SeverityTypeID::ST_MinorTrace, what, "OK", alive);
		}

		void on_connect(const boost::system::error_code& ec) {
			if (report_status(ec, "on_connect"))
				return;

			m_SslSocket.async_handshake(ssl_socket::client, [self = shared_from_this()](const boost::system::error_code& ec)
				{ 
					auto owner = self->m_Owner.lock();
					if (owner)  self->on_handshake(ec);
				}
			);
		}

		void on_handshake(const boost::system::error_code& ec) {
			if (report_status(ec, "on_handshake"))
				return;

			m_Request.prepare_payload();

			// Send the HTTP request to the remote host
			auto request_msg = "request: " + m_Target;
			report(MsgCategory::background_layer_request, SeverityTypeID::ST_MajorTrace, request_msg.c_str(), ", result: OK", true);
			http::async_write(m_SslSocket, m_Request, [self = shared_from_this()](const boost::system::error_code& ec, std::size_t bytes_transferred) 
				{ 
					auto owner = self->m_Owner.lock();
					if (owner)  self->on_write(ec, bytes_transferred);
				}
			);

			SetTimer();
		}

		void on_write(boost::system::error_code const& ec, std::size_t bytes_transferred)
		{
			if (report_status(ec, "on_write"))
				return;

			http::async_read(m_SslSocket, m_Buffer, m_Response, [self = shared_from_this()](boost::system::error_code const& ec, std::size_t bytes_transferred)
				{
					auto owner = self->m_Owner.lock();
					if (owner)  self->on_read(ec, bytes_transferred);
				}
			);

			SetTimer();
		}

		void on_read(boost::system::error_code ec, std::size_t size);

		std::weak_ptr<const WmsLayer> m_Owner;
		SharedStr m_Target, m_FileName;
		tile_id m_Key;
		image_format_type m_ImageFormatType;

		ssl_socket m_SslSocket;
		http::request<http::string_body> m_Request;
		http::response<http::string_body> m_Response; // Declare a container to hold the response
		boost::beast::flat_buffer m_Buffer;// This buffer is used for reading and must be persisted
		boost::asio::deadline_timer m_Timer;
	};

	std::atomic<UInt32> TileLoader::s_InstanceCount = 0;

	void ProcessPendingTasks()
	{
		while (TileLoader::s_InstanceCount) // destructor of last TileLoader, presumably called outside the run-loop, can queue new TileLoaders with new events
		{
			GetIOC()->restart(); // io_context::run() leaves the context stopped; restart before reuse
			GetIOC()->run();
			if (!TileLoader::s_InstanceCount)
				break;
		}
	}

	struct TileCache
	{
		Host m_Host;
		SharedStr m_TargetTemplStr;
		SharedStr m_FileTemplStr;
		image_format_type m_ImageFormatType;

		gdalComponent m_ReaderComponent; // should not be here.

		std::map<tile_id, image_info> m_ImageMap;
		mutable std::vector<tile_id> m_ImageStack;
		static leveled_critical_section s_ImageAccess;

		TileCache(WeakStr hostName, WeakStr targetTemplStr, WeakStr fileTemplStr, image_format_type ift)
			: m_Host(hostName.c_str())
			, m_TargetTemplStr(targetTemplStr)
			, m_FileTemplStr(fileTemplStr)
			, m_ImageFormatType(ift)
		{}
		~TileCache()
		{
#if defined(MG_DEBUG)
			for (auto t: m_ImageStack)
			{
				auto& status = m_ImageMap[t].m_Status;
//				dms_assert(status == image_status::loading);
				status = image_status::undefined;
			}
#endif
			ProcessPendingTasks();
		}
		void Status(tile_id t, image_status s)
		{
			leveled_critical_section::scoped_lock lock(s_ImageAccess);
			m_ImageMap[t].m_Status = s;
		}
		void EndLoading(tile_id t)
		{
			leveled_critical_section::scoped_lock lock(s_ImageAccess);
			if (m_ImageMap[t].m_Status == image_status::loading)
				m_ImageMap[t].m_Status = image_status::undefined;
		}

		SharedStr FileName(tile_id t) const
		{
			leveled_critical_section::scoped_lock lock(s_ImageAccess);
			return m_ImageMap.at(t).m_FileName;
		}

		bool LoadTile(const WmsLayer* layer, tile_id t)
		{
			leveled_critical_section::scoped_lock lock(s_ImageAccess);

			image_info& info = m_ImageMap[t];
			if (info.m_Status == image_status::undefined)
			{
				info.m_FileName = m_FileTemplStr
					.replace("@TM@", AsString(t.first, FormattingFlags::None).c_str())
					.replace("@TR@", AsString(t.second.Row(), FormattingFlags::None).c_str())
					.replace("@TC@", AsString(t.second.Col(), FormattingFlags::None).c_str());

				if (IsFileOrDirAccessible(info.m_FileName))
					// maybe it was already loaded by an earlier session or different View
				{
					info.m_Status = image_status::ready;
					reportF(MsgCategory::background_layer_request, SeverityTypeID::ST_MinorTrace
						, "Read from cache: %s"
						, info.m_FileName.c_str()
					);
					return true;
				}
				info.m_Status = image_status::loading;
			}
			return info.m_Status == image_status::ready; // already or just loading -> false, ready -> true
		}

		void PushTileLoad(tile_id t) const
		{
			leveled_critical_section::scoped_lock lock(s_ImageAccess);
			m_ImageStack.push_back(t);
		}

		GraphVisitState RunTileLoads(const WmsLayer* layer, bool maySuspend)
		{
			leveled_critical_section::scoped_lock lock(s_ImageAccess);

			if (TileLoader::s_InstanceCount >= WMS_MAX_CONCURRRENT_REQUESTS)
				return GVS_Yield;

			std::vector<tile_id> safedForLater;
			tile_id key;
			for (;;) {
				if (m_ImageStack.empty())
				{
					m_ImageStack.insert(m_ImageStack.end(), safedForLater.rbegin(), safedForLater.rend());
					return GVS_Ready;
				}

				key = m_ImageStack.back();
				m_ImageStack.pop_back();

				auto& imageMap = m_ImageMap.at(key);
				image_status& status = imageMap.m_Status;
				if (status != image_status::loading)
					continue;

				if (key.first == layer->m_ZoomLevel)
				{
					if ((!maySuspend) || IsIntersecting(layer->m_TMS[key.first].WorldExtents(key.second), layer->GetViewPort()->GetCurrWorldClientRect()))
					{
						auto target = m_TargetTemplStr
							.replace("@TM@", AsString(key.first, FormattingFlags::None).c_str())
							.replace("@TR@", AsString(key.second.Row(), FormattingFlags::None).c_str())
							.replace("@TC@", AsString(key.second.Col(), FormattingFlags::None).c_str());
						std::shared_ptr<TileLoader> tileLoader = std::make_shared<TileLoader>(layer, m_Host, target, m_ImageMap.at(key).m_FileName, key, m_ImageFormatType);
						tileLoader->Run(m_Host);
						return GVS_Yield;
					}
				}
				else if (!maySuspend)
					safedForLater.push_back(key);
				else				
					status = image_status::undefined; // cancel the loading of tiles no longer required
			};
		}
	};
	leveled_critical_section TileCache::s_ImageAccess(item_level_type(0), ord_level_type(2), "WmsLayer.TileCache");

	TileLoader::~TileLoader()
	{
		MG_DEBUGCODE(report(MsgCategory::background_layer_request, "dtor", false));
		--s_InstanceCount;
		auto owner = m_Owner.lock();
		if (owner)
		{
			try {
				owner->m_TileCache->EndLoading(m_Key);
				owner->RunTileLoads(true); // load next
			}
			catch (...) {}
		}
	}

	void TileLoader::report(MsgCategory ct, SeverityTypeID st, CharPtr what, CharPtr msg, bool alive)
	{
		if (st == SeverityTypeID::ST_Error)
			st = SeverityTypeID::ST_Warning;

		reportF(ct, st, "%1%:%2%\nRequest: %3%\nResponse: %4%\n" // wms(%5%,%6%)
			, what 
			, msg 
			, m_Request 
			, m_Response 
			//, s_InstanceCount s
			//, (alive ? shared_from_this().use_count()-1 : 0 )
		);
	}

	bool TileLoader::report_status(const boost::system::error_code& ec, char const* what)
	{
		m_Timer.cancel();

		if (!ec)
		{
			//report(SeverityTypeID::ST_MinorTrace, what, "OK", true);
			return false;
		}
		report(MsgCategory::background_layer_request, SeverityTypeID::ST_Warning, what, ec.message().c_str(), true);

		auto owner = m_Owner.lock();
		if (owner)
			owner->m_TileCache->Status(m_Key, image_status::undefined);
		return true;
	}

	// One-shot synchronous HTTPS GET of a legend image into a local cache file
	// (issue #405). Legends are small and fetched only when the user reveals the
	// legend, so a blocking fetch on the GUI thread is acceptable; the cached file
	// is then decoded by the same GDAL reader used for the tiles.
	bool FetchUrlToFile(const SharedStr& hostName, const SharedStr& target, const SharedStr& localFile)
	{
		try {
			auto ioc = GetIOC();
			ssl::context ctx(boost::asio::ssl::context::sslv23);
			ctx.set_default_verify_paths();
			ctx.set_verify_mode(ssl::verify_none);

			tcp::resolver resolver(*ioc);
			auto results = resolver.resolve(hostName.c_str(), "https");

			ssl_socket stream(*ioc, ctx);
			stream.set_verify_mode(ssl::verify_none);
			boost::asio::connect(stream.lowest_layer(), results.begin(), results.end());
			stream.handshake(ssl::stream_base::client);

			http::request<http::empty_body> req{ http::verb::get, target.c_str(), 11 };
			req.set(http::field::host, hostName.c_str());
			req.set(http::field::user_agent, "GeoDMS");
			req.set(http::field::connection, "close");
			http::write(stream, req);

			boost::beast::flat_buffer buffer;
			http::response<http::dynamic_body> res;
			http::read(stream, buffer, res);

			boost::system::error_code ec;
			stream.shutdown(ec); // many servers close without a clean TLS shutdown; ignore ec

			if (res.result() != http::status::ok)
			{
				reportF(MsgCategory::background_layer_request, SeverityTypeID::ST_Warning
					, "Legend fetch https://%s%s returned HTTP status %d"
					, hostName.c_str(), target.c_str(), int(res.result_int()));
				return false;
			}

			auto body = boost::beast::buffers_to_string(res.body().data());
			if (body.empty())
				return false;

			MakeDirsForFile(localFile);
			FileOutStreamBuff file(localFile, false);
			file.WriteBytes(body.data(), body.size());
			return true;
		}
		catch (...) {
			catchAndReportException();
			return false;
		}
	}

	void TileLoader::on_read(boost::system::error_code ec, std::size_t size)
	{
		m_Timer.cancel();
		
		if (!ec) 
			if (m_ImageFormatType == image_format_type::png)
				if (strncmp(m_Response.body().c_str(), "\x89PNG", 4)!=0)
					ec = boost::asio::error::basic_errors::invalid_argument;

		if (report_status(ec, "on_read"))
			return;

		auto owner = m_Owner.lock();
		if (!owner)
			return;

		m_SslSocket.lowest_layer().shutdown(tcp::socket::shutdown_both);
		{
			leveled_critical_section::scoped_lock lock(TileCache::s_ImageAccess);
			image_status& curr = owner->m_TileCache->m_ImageMap[m_Key].m_Status;
//			dms_assert(curr != image_status::undefined);
			if (curr != image_status::loading)
				return;
			if (!IsFileOrDirAccessible(m_FileName))
			{
				MakeDirsForFile(m_FileName);
				FileOutStreamBuff file(m_FileName, false);
				file.WriteBytes(&*m_Response.body().data(), m_Response.body().size());
				// close file
			}
			curr = image_status::ready;
		}

		//	if (owner->GetScale() == m_Scale)
		auto dv = owner->GetDataView().lock();
		if (dv)
			dv->PostGuiOper([key = m_Key, owner_wptr = m_Owner]() {
				auto owner = owner_wptr.lock();
				if (owner && owner->m_ZoomLevel == key.first)
					owner->InvalidateWorldRect(owner->WorldExtents(key), nullptr);
		});
	}
}	// namespace wms

struct TableEntry
{
	TableEntry(const TreeItem* container, CharPtr subItemName)
		: m_Ptr(AsDynamicDataItem(container->GetConstSubTreeItemByID(GetTokenID_mt(subItemName))))
		, m_Lock((m_Ptr->PrepareData(), m_Ptr))
	{
		MG_CHECK2(m_Ptr, mySSPrintF("%s not found", subItemName).c_str());
	}
	SharedDataItemInterestPtr m_Ptr;
	DataReadLock m_Lock;
};

struct Table : std::vector<TableEntry>
{
	template <class... NameTypes>
	Table(const TreeItem* container, NameTypes&&... names)
	{
		(emplace_back(container, std::forward<NameTypes>(names)), ...);
	}
};

WmsLayer::WmsLayer(GraphicObject* owner)
	:	GridLayerBase(owner, GetStaticClass())
	,	m_ZoomLevel(UNDEFINED_VALUE(SizeT))
{
	m_State.Set(SOF_Topographic|SOF_DetailsTooLong);
	m_State.Clear(SOF_DetailsVisible);
}

WmsLayer::~WmsLayer()
{}

void WmsLayer::SetWorldCrdUnit(const AbstrUnit* WorldCrdUnit)
{
	m_WorldCrdUnit = WorldCrdUnit;
}

void WmsLayer::SetSpecContainer(const TreeItem* specContainer)
{
	dms_assert(IsMainThread());
	m_SpecContainer = specContainer;
	float ScaleCorrection = 360.0;
	SharedStr unit("");

	SharedTreeItemInterestPtr unitItem = specContainer->GetConstSubTreeItemByID(GetTokenID_mt("unit"));
	if (unitItem)
		unit = GetTheValue<SharedStr>(unitItem.get_ptr());

	SharedTreeItemInterestPtr hostItem = specContainer->GetConstSubTreeItemByID(GetTokenID_mt("host"));
	auto hostName = GetTheValue<SharedStr>(hostItem.get_ptr());

	SharedTreeItemInterestPtr targetItem = specContainer->GetConstSubTreeItemByID(GetTokenID_mt("target"));
	auto targetTemplStr = GetTheValue<SharedStr>(targetItem.get_ptr());

	SharedTreeItemInterestPtr layerItem = specContainer->GetConstSubTreeItemByID(GetTokenID_mt("layer"));
	auto layerName = AsFilename(GetTheValue<SharedStr>(layerItem.get_ptr()));

	SetThemeDisplayName(layerName);

	SharedTreeItemInterestPtr imageFormatItem = specContainer->GetConstSubTreeItemByID(GetTokenID_mt("image_format"));
	SharedStr imageFormat;
	wms::image_format_type ift = wms::image_format_type::png;
	if (imageFormatItem)
	{
		imageFormat = GetTheValue<SharedStr>(imageFormatItem.get_ptr());
		if (imageFormat == "jpeg") ift = wms::image_format_type::jpeg;
		else if (imageFormat != "png" && imageFormat != "png8") ift = wms::image_format_type::other;
	}
	else
		imageFormat = "png";

	m_TileCache = std::make_unique<wms::TileCache>(hostName, targetTemplStr, AbstrStorageManager::GetFullStorageName("", ("%LocalDataDir%/wms/"+layerName+"/@TM@/@TR@_@TC@."+imageFormat).c_str()), ift);

	// Legend (issue #405): optional `legend` item holds a full legend-image url
	// (a WMTS Style/LegendURL or a WMS GetLegendGraphic url). Split into host +
	// target (https assumed, like the tiles); fetched and decoded lazily and cached
	// as %LocalDataDir%/wms/<layer>/legend.<ext>. WMTS does not guarantee png, so
	// the extension is only a cache-name hint -- GDAL sniffs the actual format.
	SharedTreeItemInterestPtr legendItem = specContainer->GetConstSubTreeItemByID(GetTokenID_mt("legend"));
	if (legendItem)
	{
		std::string url = GetTheValue<SharedStr>(legendItem.get_ptr()).c_str();
		auto schemePos = url.find("://");
		std::string::size_type hostStart = (schemePos == std::string::npos) ? 0 : schemePos + 3;
		auto slashPos = url.find('/', hostStart);
		std::string host = url.substr(hostStart, (slashPos == std::string::npos) ? std::string::npos : slashPos - hostStart);
		std::string path = (slashPos == std::string::npos) ? std::string("/") : url.substr(slashPos);
		if (!host.empty())
		{
			m_LegendHost = SharedStr(host.c_str());
			m_LegendTarget = SharedStr(path.c_str());
			bool looksJpeg = url.find("jpeg") != std::string::npos || url.find("jpg") != std::string::npos
				|| url.find("JPEG") != std::string::npos || url.find("JPG") != std::string::npos;
			SharedStr legendExt = looksJpeg ? SharedStr("jpg") : SharedStr("png");
			m_LegendFile = AbstrStorageManager::GetFullStorageName("", ("%LocalDataDir%/wms/"+layerName+"/legend."+legendExt).c_str());
		}
	}

	SuspendTrigger::SilentBlocker block("WmsLayer::SetSpecContainer");

	SharedPtr<const TreeItem> tileMatrices = specContainer->GetConstSubTreeItemByID(GetTokenID_mt("TileMatrix"));
	{
		Table tab(tileMatrices.get(), "name", "ScaleDenominator", "LeftCoord", "TopCoord", "TileWidth", "TileHeight", "MatrixWidth", "MatrixHeight");

		auto nameArray = const_array_cast<SharedStr>(tab[0].m_Ptr)->GetLockedDataRead();
		auto sdArray = const_array_cast<Float64>(tab[1].m_Ptr)->GetLockedDataRead();
		auto lcArray = const_array_cast<Float64>(tab[2].m_Ptr)->GetLockedDataRead();
		auto tcArray = const_array_cast<Float64>(tab[3].m_Ptr)->GetLockedDataRead();
		auto twArray = const_array_cast<UInt16>(tab[4].m_Ptr)->GetLockedDataRead();
		auto thArray = const_array_cast<UInt16>(tab[5].m_Ptr)->GetLockedDataRead();
		auto mwArray = const_array_cast<UInt32>(tab[6].m_Ptr)->GetLockedDataRead();
		auto mhArray = const_array_cast<UInt32>(tab[7].m_Ptr)->GetLockedDataRead();
		SizeT size = nameArray.size();
		MG_CHECK(size == sdArray.size() && size == lcArray.size() && size == tcArray.size() && size == twArray.size() && size == thArray.size() && size == mwArray.size() && size == mhArray.size());

		CrdRect extents;
		for (SizeT i = 0; i != size; ++i)
		{
			Float64 scaleDenom = sdArray[i] * 0.00028; // size of a pixel in meters ?

			if (strncmp(unit.c_str(), "degree", 30) == 0)
				ScaleCorrection = 40075000;
			
			scaleDenom *= (360.0 / ScaleCorrection); // 360 degrees span the equator of length 40075km.
			
			m_TMS.emplace_back(wms::tile_matrix{ SharedStr(nameArray[i]), 
				CrdTransformation(shp2dms_order(lcArray[i], tcArray[i]), shp2dms_order(scaleDenom, -scaleDenom)),
				shp2dms_order(twArray[i], thArray[i]), 
				shp2dms_order(mwArray[i], mhArray[i]) 
			});
			extents |= m_TMS.back().WorldExtents();
		}
		SetWorldClientRect(extents);
	}
}

bool WmsLayer::EnsureLegendImage() const
{
	if (m_LegendState == legend_state::ready)
		return true;
	if (m_LegendState == legend_state::failed || m_LegendTarget.empty())
	{
		m_LegendState = legend_state::failed;
		return false;
	}

	// Fetch to the local cache file unless an earlier session already did.
	if (!IsFileOrDirAccessible(m_LegendFile))
	{
		if (!wms::FetchUrlToFile(m_LegendHost, m_LegendTarget, m_LegendFile))
		{
			m_LegendState = legend_state::failed;
			return false;
		}
	}

	// Decode the cached image with the same GDAL reader the tiles use.
	try {
		GDAL_SimpleReader gdalReader;
		GDAL_SimpleReader::buffer_type buffer;
		WPoint size = gdalReader.ReadGridData(m_LegendFile.c_str(), buffer);
		if (size == WPoint() || buffer.combinedBands.empty())
		{
			m_LegendState = legend_state::failed;
			return false;
		}
		m_LegendPixels = std::move(buffer.combinedBands);
		m_LegendSize = size;

		// GDAL yields top-down rows, but DrawContext::DrawImage (32bpp) expects a
		// bottom-up DIB. Flip vertically once here so a positive height renders
		// correctly on both the GDI and Qt backends.
		UInt32 w = m_LegendSize.Col(), h = m_LegendSize.Row();
		if (w > 0 && h > 0 && SizeT(w) * h == m_LegendPixels.size())
			for (UInt32 y = 0; y < h / 2; ++y)
				std::swap_ranges(
					m_LegendPixels.begin() + SizeT(y) * w,
					m_LegendPixels.begin() + SizeT(y) * w + w,
					m_LegendPixels.begin() + SizeT(h - 1 - y) * w);

		m_LegendState = legend_state::ready;
		return true;
	}
	catch (...) {
		catchAndReportException();
		m_LegendState = legend_state::failed;
		return false;
	}
}

SizeT ChooseTileMatrix(const wms::tile_matrix_set& tms, Float64 factor)
{
	SizeT focusIndex = UNDEFINED_VALUE(SizeT);
	Float64 focusFactor = 1.0;
	factor *= 1.01; // accept slightly too large factors to avoid pendantic zooming
	for (const wms::tile_matrix& tm : tms)
	{
		if (tm.m_Raster2World.ZoomLevel() <= factor)
			if ((!IsDefined(focusIndex)) || focusFactor < tm.m_Raster2World.ZoomLevel())
			{
				focusFactor = tm.m_Raster2World.ZoomLevel();
				focusIndex = &tm - &tms[0];
			}
	}
	return focusIndex;
} 

void WmsLayer::RunTileLoads(bool maySuspend) const
{
	m_TileCache->RunTileLoads(this, maySuspend);
}

bool WmsLayer::Draw(GraphDrawer& d) const
{
	if (!VisibleLevel(d))
		return GVS_Continue;
	if (!d.GetDrawContext())
		return GVS_Continue;

	ViewPort* vp = d.GetViewPortPtr();
	assert(vp);
	// For a tilted (projective) view the device-per-world scale varies across the plane, so
	// ZoomLevel()'s LocalScaleAt((0,0)) (the world origin, far outside the view) is meaningless and
	// ChooseTileMatrix would fail -> the WMS would draw nothing. Sample the scale at the VIEW CENTRE
	// instead. Axis-separable (incl. pure yaw) keeps the historical ZoomLevel(), byte-identical.
	auto viewTr = d.GetTransformation();
	auto transZoomLevel = viewTr.IsAxisSeparable() ? viewTr.ZoomLevel() : viewTr.LocalScaleAt(Center(vp->GetROI()));
	auto worldSizeOfDevicePixel = 1.0 / transZoomLevel;
	m_ZoomLevel = ChooseTileMatrix(m_TMS, worldSizeOfDevicePixel);
	if (!IsDefined(SizeT(m_ZoomLevel)))
		return GVS_Continue;

	const wms::tile_matrix& tm = m_TMS[m_ZoomLevel];
	grid_coord_key gcKey = tm.GridCoordKey();
	GridCoordPtr drawGridCoords = vp->GetOrCreateGridCoord(gcKey);

	if (vp == GetViewPort())
		m_GridCoord = drawGridCoords; // cache only if vp owns this GridLayer

	drawGridCoords->UpdateToScale(d.GetSubPixelFactors());

	GRect bb = d.GetAbsClipRegion().BoundingBox();

	auto viewportDeviceOffset = ScaleCrdPoint(d.GetClientLogicalAbsPos(), d.GetSubPixelFactors());

	// Under a rotated/tilted VIEW the composite raster->device is >c (not axis-separable), so the
	// GridCoord's separable device<->grid row/col arrays are empty. Use the composite inverse to map the
	// visible device rect back to raster space (and to place each tile) instead. grid2dev = raster->device.
	CrdTransformation grid2dev = drawGridCoords->GetGrid2DeviceTransformation();
	bool wmsSeparable = drawGridCoords->IsSeparable();

	GRect visibleRel = bb - CrdPoint2GPoint(viewportDeviceOffset); // visible device region (relative)
	GRect clippedRelRect;
	if (wmsSeparable)
	{
		clippedRelRect = drawGridCoords->GetClippedRelDeviceRect();
		if (clippedRelRect.empty())
			return GVS_Continue;
		clippedRelRect &= visibleRel;
	}
	else
		// >c: the Recalc device AABB of the WHOLE raster is unreliable under a projective transform (its
		// far corners project with w<=0), so use the actual visible device region directly.
		clippedRelRect = visibleRel;
	if (clippedRelRect.empty())
		return GVS_Continue;

	// For the rotated/tilted (>c) path the composite inverse (device->grid) bounds the tile loop and the
	// forward perspective-w sign (fwdRefSign = the sign in front of the camera) rejects behind-horizon tiles.
	CrdTransformation dev2grid;
	CrdType fwdRefSign = 1.0;

	IPoint tlPixel, brPixel;
	if (wmsSeparable)
	{
		tlPixel = drawGridCoords->GetExtGridCoord(clippedRelRect.LeftTop());
		brPixel = drawGridCoords->GetExtGridCoord(clippedRelRect.RightBottom() - GPoint(1,1) );
	}
	else
	{
		// Map the visible (rotated/tilted) device rect back to raster pixels to bound the tile loop. Under
		// TILT the far edge of the device rect can project BEYOND the projective horizon, where Reverse()
		// yields garbage that would otherwise clamp the loop to the WHOLE matrix (tile flood). So sample the
		// device rect and keep only points in front of the camera (perspective-w of the same sign as the view
		// centre); the AABB of those valid grid samples caps the extent at the horizon. For yaw/affine w is
		// constant (==1) so every sample is valid and (the rect corners being the extremes) this AABB equals
		// the old ReverseBounds clamped to the matrix - byte-identical tile range to the committed behaviour.
		dev2grid = grid2dev.Inverse();
		CrdRect  devR     = g2dms_order<CrdType>(clippedRelRect);
		CrdPoint fullSize = Convert<CrdPoint>(tm.RasterSize());
		CrdType  refSign  = (dev2grid.ApplyDenom(Center(devR)) < 0) ? -1.0 : 1.0;
		fwdRefSign        = (grid2dev.ApplyDenom(dev2grid.Apply(Center(devR))) < 0) ? -1.0 : 1.0;
		const int N = 24; // horizon resolution along each device edge
		CrdRect gridR; bool haveValid = false;
		for (int i = 0; i <= N; ++i)
			for (int j = 0; j <= N; ++j)
			{
				CrdPoint devP( devR.first.first  + (devR.second.first  - devR.first.first ) * (i / CrdType(N))
				             , devR.first.second + (devR.second.second - devR.first.second) * (j / CrdType(N)) );
				if (dev2grid.ApplyDenom(devP) * refSign <= 0) // on/behind the horizon -> skip
					continue;
				CrdPoint g = dev2grid.Apply(devP);
				g.first  = std::max(0.0, std::min(fullSize.first,  g.first )); // clamp to the matrix (a near-horizon
				g.second = std::max(0.0, std::min(fullSize.second, g.second)); // sample's grid coord can explode)
				if (!haveValid) { gridR = CrdRect(g, g); haveValid = true; }
				else
				{
					gridR.first.first   = std::min(gridR.first.first,   g.first);  gridR.first.second  = std::min(gridR.first.second,  g.second);
					gridR.second.first  = std::max(gridR.second.first,  g.first);  gridR.second.second = std::max(gridR.second.second, g.second);
				}
			}
		if (!haveValid)
			return GVS_Continue; // the entire visible rect is behind the horizon
		tlPixel = RoundDown<4>(gridR.first);
		brPixel = RoundUp  <4>(gridR.second);
	}

	auto tileSizeAsIPoint = Convert<IPoint>(tm.m_TileSize);
	wms::tile_pos tlTile = tlPixel / tileSizeAsIPoint;
	wms::tile_pos brTile = brPixel / tileSizeAsIPoint;

	assert(IsLowerBound(tlTile, brTile));
	MakeMax(tlTile, wms::tile_pos(0,0));
	MakeMin(brTile, tm.m_MatrixSize - wms::tile_pos(1,1));
	assert(IsLowerBound(tlTile, brTile));

	GDAL_SimpleReader gdalReader;
	GDAL_SimpleReader::buffer_type rasterBuffer;
	GridColorPalette palette(nullptr);

	for (auto r = tlTile.Row(); r <= brTile.Row(); ++r)
		for (auto c = tlTile.Col(); c <= brTile.Col(); ++c)
		{
			wms::tile_pos tp = shp2dms_order(c, r);
			auto tileGridRect = tm.RasterExtents(tp);
			IRect tileIR = Convert<IRect>(tileGridRect);
			GRect tileRelRect;
			if (wmsSeparable)
				tileRelRect = drawGridCoords->GetClippedRelDeviceRect(tileIR);
			else
			{
				CrdRect tileCR = Convert<CrdRect>(tileIR);
				// Skip tiles whose centre is behind the projective horizon: their forward projection has
				// w<=0 so ApplyBounds would yield a bogus device rect that could spuriously pass the clip.
				// (No-op for yaw/affine, where w==1 and fwdRefSign==1.)
				if (grid2dev.ApplyDenom(Center(tileCR)) * fwdRefSign <= 0)
					continue;
				CrdRect tdev = grid2dev.ApplyBounds(tileCR);
				IPoint lo = RoundDown<4>(tdev.first), hi = RoundUp<4>(tdev.second);
				tileRelRect = GRect(lo.Col(), lo.Row(), hi.Col(), hi.Row()) & clippedRelRect;
			}
			if (tileRelRect.empty())
				continue;

			wms::tile_id wmsTileKey(m_ZoomLevel, tp);
			if (!m_TileCache->LoadTile(this, wmsTileKey))
			{
				m_TileCache->PushTileLoad(wmsTileKey);
				for (;;) {
					auto result = m_TileCache->RunTileLoads(this, d.IsSuspendible());
					if (d.IsSuspendible())
						goto nextTile; // skip drawing this tile, its region will be invalidated when loaded asynchroneously
					wms::ProcessPendingTasks();
					if (result == GVS_Ready && !wms::TileLoader::s_InstanceCount)
					{
						if (!IsFileOrDirAccessible(m_TileCache->FileName(wmsTileKey)))
							goto nextTile;
						break;
					}
				}
			}
			try {
				auto result = gdalReader.ReadGridData(m_TileCache->FileName(wmsTileKey).c_str(), rasterBuffer);
				if (result==WPoint())
					goto nextTile;

						if (wmsSeparable)
						{
							GridDrawer drawer(
								drawGridCoords.get()
								, GetIndexCollector()
								, &palette
								, nullptr // selValues
								, d.GetDrawContext()
								, tileRelRect
								, ::tile_id(0)
								, tileIR - drawGridCoords->GetGridRect().first // adjusted tileRect
							);

							if (!drawer.empty()) {
								drawer.AllocatePixelBuffer();
								drawer.FillDirect(&rasterBuffer.combinedBands[0], rasterBuffer.combinedBands.size(), true);
								drawer.CopyToDrawContext(CrdPoint2GPoint(viewportDeviceOffset));
							}
						}
						else
						{
							// rotated/tilted view: one transformed blit of the tile's raster buffer (32bpp,
							// GDAL top-down). src2device: tile-local raster pixel -> device (= raster origin
							// translate, then raster->device, then the viewport device offset).
							int tileW = Width(tileIR), tileH = Height(tileIR);
							if (tileW > 0 && tileH > 0 && SizeT(tileW) * SizeT(tileH) <= rasterBuffer.combinedBands.size())
							{
								// src2device maps a tile-local pixel (X=col, Y=row) to device. The GDAL tile buffer is
								// row-major top-down (row 0 = north = the tile's min grid row), which is exactly what
								// the (order-independent) DrawImageTransformed expects (src32[row*tileW + col]).
								CrdTransformation src2device(Convert<CrdPoint>(tileIR.first), CrdPoint(1.0, 1.0));
								src2device *= grid2dev;
								src2device += viewportDeviceOffset;
								d.GetDrawContext()->DrawImageTransformed(src2device, rasterBuffer.combinedBands.data(), tileW, tileH, DmsRasterOp::SrcAnd);
							}
						}
			}
			catch (...)
			{
				catchAndReportException();
			}
		nextTile:;
		}
	return GVS_Continue;
}

void WmsLayer::Zoom1To1(ViewPort* vp)
{
	assert(vp);

	if (!vp->GetWorldCrdUnit())
		return;
	SizeT zoomLevel = m_ZoomLevel;
	if (!IsDefined(zoomLevel))
	{
		if (m_TMS.empty())
			return;
		zoomLevel = m_TMS.size() - 1;
	}

	CrdPoint p = Center(vp->GetROI());

	CrdPoint r2wFactor = m_TMS[zoomLevel].m_Raster2World.Factor();
	CrdPoint currClientSize = Convert<CrdPoint>(vp->GetCurrClientSize());
	CrdPoint sf = GetScaleFactors();
	CrdPoint s = r2wFactor * currClientSize * sf * 0.5;
	vp->SetROI(CrdRect(p - s, p + s));
}

bool WmsLayer::ZoomOut(ViewPort* vp, bool justClickIsOK)
{
	assert(vp);

	auto zoomFactor = vp->CalcCurrWorldToDeviceZoomLevel();
	m_ZoomLevel = ChooseTileMatrix(m_TMS, 1.0 / zoomFactor);
	if (!m_ZoomLevel)
		return false;
	if (m_TMS.size() < SizeT(m_ZoomLevel))
		m_ZoomLevel = SizeT(m_TMS.size());
	--m_ZoomLevel;
	Zoom1To1(vp);
//	m_ZoomLevel = ChooseTileMatrix(m_TMS, 1.0 / zoomLevel);
	if (!zoomFactor || vp->CalcCurrWorldToDeviceZoomLevel() / zoomFactor < 0.99 || justClickIsOK)
		return true;
	if (!m_ZoomLevel)
		return false;
	--m_ZoomLevel;
	Zoom1To1(vp);
	return true;
}

bool WmsLayer::ZoomIn(ViewPort* vp)
{
	assert(vp);

	auto zoomFactor = vp->CalcCurrWorldToDeviceZoomLevel();
	m_ZoomLevel = ChooseTileMatrix(m_TMS, 1.0 / zoomFactor);
	if (m_ZoomLevel >= m_TMS.size() - 1)
		return false;
//	++m_ZoomLevel;
	Zoom1To1(vp);
	//	m_ZoomLevel = ChooseTileMatrix(m_TMS, 1.0 / zoomLevel);
	if (!zoomFactor)
		return true;
	auto currW2DevZL= vp->CalcCurrWorldToDeviceZoomLevel();
	if (currW2DevZL / zoomFactor > 1.01)
		return true;
	if (m_ZoomLevel >= m_TMS.size() - 1)
		return false;
	++m_ZoomLevel;
	Zoom1To1(vp);
	return true;
}

bool WmsLayer::ZoomToFinestLevel(ViewPort* vp)
{
	assert(vp);
	if (m_TMS.empty())
		return false;
	m_ZoomLevel = m_TMS.size() - 1; // deepest = highest available zoom level
	Zoom1To1(vp);                   // sizes the ROI to that level, centered on the current ROI center
	return true;
}

void WmsLayer::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	base_type::Sync(viewContext, sm);
	if (sm == SM_Load)
	{
		SharedPtr<const TreeItem> specContainer;
		SyncRef(specContainer, viewContext, GetTokenID_mt("SpecContainer"), sm);
		if (specContainer)
			SetSpecContainer(specContainer.get());
	}
	else
		SyncRef(m_SpecContainer, viewContext, GetTokenID_mt("SpecContainer"), sm);
}

IMPL_DYNC_LAYERCLASS(WmsLayer, ASE_BrushColor, AN_BrushColor, 2)

