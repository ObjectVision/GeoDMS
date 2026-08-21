// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
// ShvSignal.h : minimal synchronous signal/slot mechanism for shv view components.
//
// Hand-rolled replacement for boost::signals2. Deliberately single-threaded: all
// connects, emissions, and disconnects happen on the GUI thread.
//
// Semantics preserved from boost::signals2:
// - connect() returns a SignalConnection; discarding it leaves the slot connected for
//   the remaining lifetime of the signal (see GraphicLayer's self-subscription).
// - ScopedConnection disconnects its slot on destruction and on (re)assignment.
// - Emission invokes slots synchronously, in connection order.
// - A slot disconnected during an emission is not invoked later in that same emission;
//   a slot connected during an emission is only invoked from the next emission on.
// - Disconnecting or destroying a connection after the signal itself was destroyed is
//   a safe no-op: connections track their slot through a std::weak_ptr.

#if !defined(__SHV_SIGNAL_H)
#define __SHV_SIGNAL_H

#include "cpc/Types.h"

#include <functional>
#include <memory>
#include <vector>

class SignalConnection
{
	template <typename... Args> friend class Signal;

	explicit SignalConnection(std::weak_ptr<bool> connectedFlag)
		:	m_ConnectedFlag(std::move(connectedFlag))
	{}

public:
	SignalConnection() = default;

	void disconnect()
	{
		if (auto connectedFlag = m_ConnectedFlag.lock())
			*connectedFlag = false;
		m_ConnectedFlag.reset();
	}

private:
	std::weak_ptr<bool> m_ConnectedFlag; // aliases the Slot's m_Connected flag; expires when the signal releases the slot
};

template <typename... Args>
class Signal
{
	struct Slot
	{
		explicit Slot(std::function<void(Args...)> func) : m_Func(std::move(func)) {}

		std::function<void(Args...)> m_Func;
		bool                         m_Connected = true;
	};

public:
	Signal() = default;
	Signal(Signal&&) noexcept = default;
	Signal& operator=(Signal&&) noexcept = default;
	Signal(const Signal&) = delete;
	Signal& operator=(const Signal&) = delete;

	SignalConnection connect(std::function<void(Args...)> func)
	{
		std::erase_if(m_Slots, [](const auto& slot) { return !slot->m_Connected; });
		auto slot = std::make_shared<Slot>(std::move(func));
		m_Slots.push_back(slot);
		return SignalConnection(std::shared_ptr<bool>(slot, &slot->m_Connected));
	}

	void operator()(Args... args) const
	{
		// snapshot keeps the executing slots alive and the iteration valid, even when a
		// slot connects or disconnects slots, or destroys this signal, during emission.
		// NB: identifiers named slots/signals/emit are erased by Qt's keyword macros,
		// which reach shv TUs through Region.h's <QRegion>/<QRect> includes.
		auto slotsSnapshot = m_Slots;
		for (const auto& slot : slotsSnapshot)
			if (slot->m_Connected)
				slot->m_Func(args...);
	}

private:
	std::vector<std::shared_ptr<Slot>> m_Slots;
};

class ScopedConnection
{
public:
	ScopedConnection() = default;
	ScopedConnection(SignalConnection conn) : m_Connection(std::move(conn)) {}
	ScopedConnection(ScopedConnection&& rhs) noexcept : m_Connection(std::move(rhs.m_Connection)) {}

	ScopedConnection(const ScopedConnection&) = delete;
	ScopedConnection& operator=(const ScopedConnection&) = delete;

	~ScopedConnection() { disconnect(); }

	ScopedConnection& operator=(ScopedConnection&& rhs) noexcept
	{
		if (this != &rhs)
		{
			disconnect();
			m_Connection = std::move(rhs.m_Connection);
		}
		return *this;
	}

	ScopedConnection& operator=(SignalConnection conn)
	{
		disconnect();
		m_Connection = std::move(conn);
		return *this;
	}

	void disconnect() { m_Connection.disconnect(); }

private:
	SignalConnection m_Connection;
};

using CmdSignal              = Signal<>;
using FocusElemChangedSignal = Signal<SizeT, SizeT>;

#endif // !defined(__SHV_SIGNAL_H)
