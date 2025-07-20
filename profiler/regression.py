import os
import platform
import importlib.util
import sys
from packaging.version import Version
import glob
from bs4 import BeautifulSoup
import filecmp
import webbrowser

def get_empty_table_row_col_html() -> str:
    return '<td style="border-right: 0px; border-bottom: 1px solid @@@COLOR@@@; box-shadow: 0 1px 0 #FFFFFF; padding: 0px;"></td>\n'

def get_table_row_col_color(status:str) -> str:
    if status == "OK":
        return "#90ee90"
    if status == "TIMEOUT":
        return "#ccddee"
    return "#ff6f6f"

def get_days_hours_minutes_seconds_from_duration(duration:int):
    # split duration [s] into components
    time = duration
    day = time // (24 * 3600)
    time = time % (24 * 3600)
    hour = time // 3600
    time %= 3600
    minutes = time // 60
    time %= 60
    seconds = time
    return day, hour, minutes, seconds

def get_indicator_part_from_parsed_results(parsed_results:dict)->str:
    indicator_part = ""
    for indicator in parsed_results:
        if indicator == "result":
            continue
        value = parsed_results[indicator]
        #nr_inhabitants: <B>1337</B><BR>
        indicator_part += f"{indicator}: <B>{value}</B><BR>"
    return indicator_part

def get_table_regression_test_row(result_paths:dict, summary_row:list) -> str:
    regression_test_row = get_table_row_title_html_template()
    testname = summary_row[0]
    testclass = testname.replace(" ", "_")
    regression_test_row = regression_test_row.replace("@@@TESTNAME@@@", testname)
    regression_test_row = regression_test_row.replace("@@@TESTCLASS@@@", testclass)
    for summary_col_row in summary_row[1:]:
        if not summary_col_row:
            regression_test_row += get_empty_table_row_col_html()
            continue
        table_col_header = get_table_row_col_html_template(result_paths, summary_col_row["log_filename"], summary_col_row["profile_figure_filename"])
        status = summary_col_row["status"]
        color = get_table_row_col_color(status)
        table_col_header = table_col_header.replace("@@@TESTCLASS@@@", testclass)
        table_col_header = table_col_header.replace("@@@STATUS@@@", status)
        table_col_header = table_col_header.replace("@@@COLOR@@@", color)
        day, hour, minutes, seconds = get_days_hours_minutes_seconds_from_duration(summary_col_row["duration"])

        #2025 05 21 : 12.24.32
        command = summary_col_row["command"]#.replace("GeoDmsRun.exe", "GeoDmsGuiQt.exe")
        table_col_header = table_col_header.replace("@@@GEODMS_CMD@@@", command)
        table_col_header = table_col_header.replace("@@@STARTTIME@@@", str(summary_col_row["start_time"].strftime("%Y %m %d %H:%M:%S")))
        table_col_header = table_col_header.replace("@@@DAYS@@@", str(int(day)))
        table_col_header = table_col_header.replace("@@@HOURS@@@", str(int(hour)))
        table_col_header = table_col_header.replace("@@@MINS@@@", str(int(minutes)))
        table_col_header = table_col_header.replace("@@@SECONDS@@@", str(int(seconds)))
        table_col_header = table_col_header.replace("@@@HIGHESTCOMMIT@@@", f"{summary_col_row["highest_commit"]:.3f}")
        table_col_header = table_col_header.replace("@@@MAXTHREADS@@@", str(summary_col_row["max_threads"]))
        table_col_header = table_col_header.replace("@@@TOTALREAD@@@", f"{summary_col_row["total_read"]:.5f}")
        table_col_header = table_col_header.replace("@@@TOTALWRITE@@@", f"{summary_col_row["total_write"]:.5f}")
        table_col_header = table_col_header.replace("@@@LOG@@@", summary_col_row["log_filename"])
        table_col_header = table_col_header.replace("@@@PROFILE_FIGURE@@@", summary_col_row["profile_figure_filename"])

        # indicators        
        indicator_part = get_indicator_part_from_parsed_results(summary_col_row["results"][1])
        table_col_header = table_col_header.replace("@@@INDICATORS@@@", indicator_part)

        regression_test_row += table_col_header

    return f'<tr style="background-color: #EEEEFF">{regression_test_row}</tr>\n'

def get_table_row_title_html_template() -> str:
    return '<td style="border-right: 0px; border-bottom: 1px solid #BEBEE6; box-shadow: 0 1px 0 #FFFFFF; padding: 0px;white-space:nowrap">\
                <button onclick="expand_test_row(\'@@@TESTCLASS@@@\')" style="border:0px transparent; background-color: transparent;">@@@TESTNAME@@@</button>\
            </td>\n'

def get_table_row_col_html_template(result_paths:dict, log_fn:str=None, profile_fig_fn:str=None) -> str:
    absolute_log_fn = f"{result_paths["results_base_folder"]}/{log_fn[3:]}"
    absolute_profile_fn = f"{result_paths["results_base_folder"]}/{profile_fig_fn[3:]}"
    
    log_part = "" if not os.path.isfile(absolute_log_fn) else '<a href="@@@LOG@@@" target="_blank"><span class="material-symbols-outlined">article</span></a>'
    profile_part = "" if not os.path.isfile(absolute_profile_fn) else '<a href="@@@PROFILE_FIGURE@@@" target="_blank"><span class="material-symbols-outlined">timeline</span></a>'
    geodms_part = '<a href="@@@GEODMS_CMD@@@" onclick="copy_href(event, this)"><span class="material-symbols-outlined">globe</span></a>'
    return f'<td style="border-right: 0px; border-bottom: 1px solid @@@COLOR@@@; background-color:@@@COLOR@@@; box-shadow: 0 1px 0 #FFFFFF; padding: 0px; white-space: nowrap">\
    <details class=@@@TESTCLASS@@@>\
    <summary>@@@STATUS@@@</summary>\
    start: @@@STARTTIME@@@<BR>\
    duration: <B>@@@DAYS@@@</B>d<B> @@@HOURS@@@</B>h<B> @@@MINS@@@</B>m<B> @@@SECONDS@@@</B>s<BR>\
    highest Commit: <B>@@@HIGHESTCOMMIT@@@[GB]</B><BR>\
    max threads: <B>@@@MAXTHREADS@@@</B><BR>\
    total read: <B>@@@TOTALREAD@@@[GB]</B><BR>\
    total write: <B>@@@TOTALWRITE@@@[GB]</B><BR>\
    @@@INDICATORS@@@\
    </details>\
    {log_part} {geodms_part} {profile_part}\
    </td>\n'

def collect_experiment_summaries(version_range:tuple, result_paths:dict, sorted_valid_result_folders:list, regression_test_names:list, regression_test_files:dict) -> list[list]:
    # initialize table
    rows = len(regression_test_names)+1
    cols = len(sorted_valid_result_folders)+1
    summaries = [[None for _ in range(cols)] for _ in range(rows)]

    summaries[0][0] = f"geodms regression test results:<br> {version_range[0]}...{version_range[1]}"

    # fill table with summaries
    for regression_test in regression_test_files.keys():
        row = get_result_row(regression_test, regression_test_names)
        summaries[row][0] = regression_test.replace("_", " ")
        binary_experiment_result_files = regression_test_files[regression_test]
        regression_test_experiments = []
        for experiment_file in binary_experiment_result_files:
            col = get_result_col(experiment_file, sorted_valid_result_folders)
            if not summaries[0][col]:
                summaries[0][col] = get_col_header(col, sorted_valid_result_folders)
            experiment = Profiler.loadExperimentFromPickleFile(None, experiment_file)
            summaries[row][col] = experiment.summary()
            regression_test_experiments.append(experiment)
            log_filename = get_log_filename(sorted_valid_result_folders[col-1][0], regression_test)
            profile_fig_filename = get_profile_figure_filename(sorted_valid_result_folders[col-1][0], regression_test)
            summaries[row][col]["profile_figure_filename"] = f"../{profile_fig_filename}"
            summaries[row][col]["log_filename"] = f"../{log_filename}"
            status_code = experiment.result["status_code"] if "status_code" in experiment.result else 0
            results = get_regression_test_result(status_code, regression_test, f"{result_paths["results_base_folder"]}/{sorted_valid_result_folders[col-1][0]}", experiment.file_comparison)
            summaries[row][col]["status"] = results[0]
            summaries[row][col]["results"] = results
        
        target_visualized_experiments_filename = get_profile_figure_filename(result_paths["results_folder"], regression_test)
        if not os.path.exists(target_visualized_experiments_filename):
            visualized_experiments_filename = Profiler.VisualizeExperiments(regression_test_experiments, show_figure=False)
            #os.remove(target_visualized_experiments_filename)
            os.rename(visualized_experiments_filename, target_visualized_experiments_filename)

    # get column total duration and success ratio
    for col in range(1, cols):
        total_tests = rows - 1
        total_duration = 0
        succeeded = 0
        for row in range(1, rows):
            if not summaries[row][col]:
                total_tests -= 1
                continue
            summary_row_col = summaries[row][col]
            total_duration += summary_row_col["duration"]
            if summaries[row][col]["status"] == "OK":
                succeeded += 1
        summaries[0][col]["total_duration"] = total_duration
        summaries[0][col]["success_ratio"] = (succeeded, total_tests)
    return summaries

def parse_regression_test_status_file(status_filename:str) -> dict:
    # <description>operator test</description><size>number unique tests: 1356</size><result>OK</result>
    raw_html = ""
    result_dict = {}
    with open(status_filename, "r") as f:
        raw_html = f.read()
    soup = BeautifulSoup(raw_html)
    for child in soup.children:
        result_dict[child.name] = child.text
    return result_dict

def get_filepairs(benchmark_files:list, generated_files:list) -> list:
    file_pairs = []
    for benchmark_file in benchmark_files:
        benchmark_filename = os.path.basename(benchmark_file)
        for index, generated_file in enumerate(generated_files):
            generated_filename = os.path.basename(generated_file)
            if benchmark_filename ==  generated_filename:
                file_pairs.append((benchmark_file, generated_file))
            if index == len(generated_files)-1:
                file_pairs.append((benchmark_file, None))
    return file_pairs

def compare_files(file_comparison:tuple):
    benchmark_files = glob.glob(file_comparison[0])
    generated_files = glob.glob(file_comparison[1])
    filepairs = get_filepairs(benchmark_files, generated_files)

    for benchmark_file, generated_file in filepairs:
        if not generated_file:
            return False
        files_are_similar = filecmp.cmp(benchmark_file, generated_file)
        if not files_are_similar:
            return False        
    return True

def get_regression_test_result(status_code:int, regression_test:str, regression_test_folder:str, file_comparison:tuple) -> tuple:
    regression_test_status_filename_txt = f"{regression_test_folder}/{regression_test}.txt"
    regression_test_status_filename_xml = f"{regression_test_folder}/{regression_test}.xml"
    regression_test_status_filename = regression_test_status_filename_txt
    if not os.path.isfile(regression_test_status_filename):
        regression_test_status_filename = regression_test_status_filename_xml
        
    if status_code == 15:
        return ("TIMEOUT", {})

    if status_code != 0:
        return (str(status_code), {})

    if file_comparison:
        files_are_comparable = compare_files(file_comparison)
        return ("OK", {}) if files_are_comparable else ("FCFAIL", {})

    if not os.path.isfile(regression_test_status_filename):
        return ("OK", {})
    
    parsed_status_file = parse_regression_test_status_file(regression_test_status_filename)
    result_text = parsed_status_file["result"]
    if len(result_text)>15:
        
        #if "OK" in result_text:
        print(f"Compressing geodms result_text from '{result_text}' to 'OK'")
        result_text = "OK"
    return (result_text, parsed_status_file)

def get_log_filename(result_folder:str, regression_test:str):
    return f"{result_folder}/log/{regression_test}.txt"

def get_profile_figure_filename(result_folder:str, regression_test:str):
    return f"{result_folder}/{regression_test}.html"

def get_col_header(col:int, sorted_valid_result_folders:list) -> dict:
    result_folder_name, _ = sorted_valid_result_folders[col-1]
    major, minor, patch, architecture, sf, multithreading, local_machine_name = parse_folder_name(result_folder_name)
    return {"version":f"{major}.{minor}.{patch}", "build":"Release", "platform":architecture, "multi_tasking":multithreading, "computer_name":local_machine_name}

def get_result_col(experiment_file:str, sorted_valid_result_folders:list):
    col = 1
    experiment_filename = os.path.basename(experiment_file)
    foldername_from_experiment_file = experiment_filename.split("__")[0]
    for foldername, _ in sorted_valid_result_folders:
        if foldername == foldername_from_experiment_file:
            return col
        col+=1

    raise("col out of range regression: {col}")

def get_result_row(regression_test:str, regression_test_names:list):
    row = 1
    for testname in regression_test_names:
        if testname == regression_test:
            return row
        row+=1
    return row

def collect_experiment_filenames_per_experiment(regression_tests:list, result_paths:dict, sorted_valid_result_folders:list) -> dict:
    regression_tests_experiment_filenames = {}
    for regression_test in regression_tests:
        regression_tests_experiment_filenames[regression_test] = []
        for experiment_folder, _ in sorted_valid_result_folders:
            experiment_folder_path = f"{result_paths["results_base_folder"]}/{experiment_folder}"
            experiment_filenames = get_all_experiments_from_experiment_folder(experiment_folder_path)
            for experiment_filename in experiment_filenames:
                experiment_name = get_experiment_name_from_experiment_filename(experiment_filename)
                if not experiment_name == regression_test:
                    continue
                regression_tests_experiment_filenames[regression_test].append(experiment_filename)
    return regression_tests_experiment_filenames

def get_all_regression_tests_by_name(result_paths:dict, valid_result_folders:list):
    regression_tests = []
    for result_folder in valid_result_folders:
        experiment_folder_path = f"{result_paths["results_base_folder"]}/{result_folder}"
        experiment_filenames = get_all_experiments_from_experiment_folder(experiment_folder_path)
        for experiment_filename in experiment_filenames:
            experiment_name = get_experiment_name_from_experiment_filename(experiment_filename)
            if experiment_name in regression_tests:
                continue
            regression_tests.append(experiment_name)
    return regression_tests

def sort_valid_result_folders_new_to_old(valid_result_folders:list) -> list:
    sorted_valid_result_folders = []
    for result_folder in valid_result_folders:
        sorted_valid_result_folders.append((result_folder, Version(get_semantic_version_from_folder_name(result_folder))))
        sorted_valid_result_folders.sort(reverse=True, key=lambda x: x[1])
    return sorted_valid_result_folders

def get_all_experiments_from_experiment_folder(experiment_folder_path:str):
    return glob.glob(f"{experiment_folder_path}/*.bin")

def get_experiment_name_from_experiment_filename(experiment_filename:str) -> str:
    return experiment_filename.split("__")[1][:-4]

def get_valid_result_folders(version:str, result_paths:dict) -> list:
    valid_result_folders = []
    result_folder_candidates = os.listdir(result_paths["results_base_folder"])
    for candidate in result_folder_candidates:
        if not folder_is_results_folder(candidate):
            continue
        major, minor, patch, architecture, sf, multithreading, local_machine_name = parse_folder_name(candidate)
        if Version(f"{major}.{minor}.{patch}") <= Version(version):
            valid_result_folders.append(candidate)

    return valid_result_folders

def folder_is_results_folder(result_folder_name:str) -> bool:
    # valid: 17_4_5_x64_SF_C1C2C3_OVSRV07
    split_result_folder_name = result_folder_name.split("_")
    if len(split_result_folder_name)!=7:
        return False
    major, minor, patch, architecture,_,statusflags,machine_name = split_result_folder_name
    return major.isdigit() and minor.isdigit() and patch.isdigit()

def parse_folder_name(result_folder_name:str) -> list:
    major, minor, patch, architecture, sf, multithreading, local_machine_name = result_folder_name.split("_")
    return [major, minor, patch, architecture, sf, multithreading, local_machine_name]

def get_semantic_version_from_folder_name(result_folder_name:str):
    major, minor, patch,_,_,_,_ = parse_folder_name(result_folder_name)
    return f"{major}.{minor}.{patch}"

def get_version_range(valid_result_folders:list) -> tuple:
    newest_tested_geodms_version = get_semantic_version_from_folder_name(valid_result_folders[0])
    oldest_tested_geodms_version = newest_tested_geodms_version
    for result_folder_name in valid_result_folders:
        version = get_semantic_version_from_folder_name(result_folder_name)
        if Version(version) > Version(newest_tested_geodms_version):
            newest_tested_geodms_version = version
        if Version(version) < Version(oldest_tested_geodms_version):
            oldest_tested_geodms_version = version
    return (newest_tested_geodms_version, oldest_tested_geodms_version)

def get_full_regression_test_environment_string(local_machine_parameters:dict, geodms_paths:dict, regression_test_paths:dict, result_paths:dict) -> str:
    full_regression_test_string = ""
    for key in local_machine_parameters:
        value = local_machine_parameters[key]
        full_regression_test_string = f"{full_regression_test_string};{key}={value}"

    for key in geodms_paths:
        value = geodms_paths[key]
        full_regression_test_string = f"{full_regression_test_string};{key}={value}"
    
    for key in regression_test_paths:
        value = regression_test_paths[key]
        full_regression_test_string = f"{full_regression_test_string};{key}={value}"
    
    for key in result_paths:
        value = result_paths[key]
        full_regression_test_string = f"{full_regression_test_string};{key}={value}"

    return full_regression_test_string[1:]

def import_module_from_path(path):
    module_name = os.path.splitext(os.path.basename(path))[0]  # Extract "module" from "module.py"

    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None:
        raise ImportError(f"Can't find spec for {module_name} at {path}")
    
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    globals()[module_name] = module  # Inject into global namespace
    spec.loader.exec_module(module)
    
    return module

def get_geodms_paths(version:str) -> dict:
    assert(version)
    geodms_profiler_env_key = f"%GeodmsProfiler%"
    geodms_profiler = os.path.expandvars(geodms_profiler_env_key)
    geodms_paths = {}
    geodms_paths["GeoDmsPlatform"] = "x64"
    geodms_paths["GeoDmsPath"] = os.path.expandvars(f"%ProgramFiles%/ObjectVision/GeoDms{version}")
    geodms_paths["GeoDmsProfilerPath"] = geodms_profiler if geodms_profiler_env_key!=geodms_profiler else f"{geodms_paths["GeoDmsPath"]}/Profiler.py"
    geodms_paths["GeoDmsRunPath"] = f"{geodms_paths["GeoDmsPath"]}/GeoDmsRun.exe"
    geodms_paths["GeoDmsGuiQtPath"] = f"{geodms_paths["GeoDmsPath"]}/GeoDmsGuiQt.exe"
    return geodms_paths

def get_result_folder_name(version:str, geodms_paths:dict, MT1:str, MT2:str, MT3:str) -> str:
    return f"{version.replace(".", "_")}_{geodms_paths["GeoDmsPlatform"]}_SF_{MT1}{MT2}{MT3}_{get_local_machine_name()}"

def get_result_paths(geodms_paths:dict, regression_test_paths:dict, version:str, MT1:str, MT2:str, MT3:str) -> dict:
    result_paths = {}
    result_paths["results_base_folder"] = f"{regression_test_paths["TstDir"]}/Regression/GeoDMSTestResults"
    result_paths["results_folder"] = f"{result_paths["results_base_folder"]}/{get_result_folder_name(version, geodms_paths, MT1, MT2, MT3)}"
    result_paths["results_log_folder"] = f"{result_paths["results_folder"]}/log"
    return result_paths

def get_local_machine_name() -> str:
    return platform.node()

def header_stuff_to_be_removed_in_future(local_machine_parameters:dict, result_paths:dict, MT1:str, MT2:str, MT3:str):
    """
    needed for:
    regression/cfg/stam.dms /results/VersionInfo/results_folder results/VersionInfo/all /results/VersionInfo/ComputerName /results/VersionInfo/RegionalSettings
    operator/cfg/operator.dms /results/Regression/results_folder /results/Regression/t010_operator_test
    """
    local_machine_name = get_local_machine_name()
    date_format = "YYYYMMDD"
    status_flags = f"{MT1}{MT2}{MT3}"

    if not os.path.exists(local_machine_parameters["tmpFileDir"]):
        os.makedirs(local_machine_parameters["tmpFileDir"])

    with open(f"{local_machine_parameters["tmpFileDir"]}/computername.txt", "w") as f:
        f.write(local_machine_name)

    with open(f"{local_machine_parameters["tmpFileDir"]}/date_format.txt", "w") as f:
        f.write(date_format)

    with open(f"{local_machine_parameters["tmpFileDir"]}/statusflags.txt", "w") as f:
        f.write(status_flags)

    with open(f"{local_machine_parameters["tmpFileDir"]}/results_folder.txt", "w") as f:
        f.write(result_paths["results_folder"])
    return

def get_table_title_html_template() -> str:
    return '<td style="border-right: 0px; border-bottom: 1px solid #BEBEE6; box-shadow: 0 1px 0 #FFFFFF; padding: 0px;"><H4>@@@TITLE@@@</H4></td>\n'

def get_table_col_header_html_template() -> str:
    #<td style="border-right: 0px; border-bottom: 1px solid #BEBEE6; box-shadow: 0 1px 0 #FFFFFF; padding: 5px;"><I>version</I>: <B>17.4.6</B><BR><I>build</I>: <B>Release</B><BR><I>platform</I>: <B>x64</B><BR><I>multi tasking</I>: <B>S1S2S3</B><BR> 			<I>operating system</I>: <B>Windows 10</B><BR> 			<I>computername</I>: <B>OVSRV07</B><BR> </td>
    return '<td style="border-right: 0px; border-bottom: 1px solid #BEBEE6; box-shadow: 0 1px 0 #FFFFFF; padding: 0px;"><B>@@@VERSION@@@</B><BR>\
    <B>@@@MULTITASKING@@@</B><BR>\
    <B>@@@TOTALTIME@@@</B><BR>\
    <B>@@@SUCCESSRATIO@@@</B></td>\n'

    #'<td style="border-right: 0px; border-bottom: 1px solid #BEBEE6; box-shadow: 0 1px 0 #FFFFFF; padding: 0px;"><B>@@@VERSION@@@</B><BR>\
    #<B>Release</B><BR>\
    #<B>@@@PLATFORM@@@</B><BR>\
    #<B>@@@MULTITASKING@@@</B><BR>\
    #<B>@@@COMPUTER_NAME@@@</B><BR> </td>\n'

def get_table_header_row(summary_row:list) -> str:
    table_header_row = get_table_title_html_template()
    table_header_row = table_header_row.replace("@@@TITLE@@@", summary_row[0])
    for summary_col_header in summary_row[1:]:
        table_col_header = get_table_col_header_html_template()
        table_col_header = table_col_header.replace("@@@VERSION@@@", summary_col_header["version"])
        table_col_header = table_col_header.replace("@@@PLATFORM@@@", summary_col_header["platform"])
        table_col_header = table_col_header.replace("@@@MULTITASKING@@@", summary_col_header["multi_tasking"])
        days, hours, minutes, seconds = get_days_hours_minutes_seconds_from_duration(summary_col_header["total_duration"])
        table_col_header = table_col_header.replace("@@@TOTALTIME@@@", f"{int(days)} {int(hours)}:{int(minutes)}:{int(seconds)}")
        table_col_header = table_col_header.replace("@@@SUCCESSRATIO@@@", f"{summary_col_header["success_ratio"][0]}/{summary_col_header["success_ratio"][1]}")
        table_col_header = table_col_header.replace("@@@COMPUTER_NAME@@@", summary_col_header["computer_name"])
        table_header_row += table_col_header
        
    return f'<tr style="background-color: #fff497">{table_header_row}</tr>'

def get_table_rows(result_paths:dict, regression_test_summaries:list[list]) -> str:
    rows = ""    
    for index, summary_row in enumerate(regression_test_summaries):
        if index == 0:
            rows += get_table_header_row(summary_row)
            continue
        rows += get_table_regression_test_row(result_paths, summary_row)
    return rows

def render_regression_test_result_html(version_range:tuple, result_paths:dict, regression_test_summaries:dict) -> str:
    result_html = '<!DOCTYPE html>\
    <html>\
        <link href="https://fonts.googleapis.com/css2?family=Material+Symbols+Outlined" rel="stylesheet" />\
        <head>\
            <meta charset="UTF-8">\
        </head>\
        <body>\
            <script>\
                function copy_href(event, element) {\
                event.preventDefault(); \
                const rawPath = element.getAttribute("href");\
                navigator.clipboard.writeText(rawPath)\
                    .then(() => {\
                    alert("Copied the path: " + rawPath);\
                    })\
                    .catch(err => {\
                    console.error("Failed to copy path: ", err);\
                    });\
                }\
                function expand_test_row(element_class_name) {\
                    console.log("TEST");\
                    var elements = document.getElementsByClassName(element_class_name);\
                    const attribute_name = "open";\
                    console.log(elements);\
                    for (const sum_det_element of elements)\
                    {\
                        if (sum_det_element==null) {\
                            continue; \
                        }\
                        const attribute = sum_det_element.getAttribute(attribute_name);\
                        if (attribute == null) {\
                            sum_det_element.setAttribute(attribute_name, "");\
                            console.log("open");\
                        }\
                        else {\
                            sum_det_element.removeAttribute(attribute_name);\
                            console.log("close");\
                        }\
                    }\
                }\
            </script>\
            <table style="border: 0; background-color: #ddd;">\
                @@@TABLE_CONTENT@@@\
            </Table>\
        </body>\
    </html>'

    #//sum_det_element.setAttribute(attribute_name, "");\
    #//sum_det_element.removeAttribute(attribute_name);\
    table_content = get_table_rows(result_paths, regression_test_summaries)
    result_html = result_html.replace("@@@TABLE_CONTENT@@@", table_content)

    final_html_filename = f"{result_paths['results_base_folder']}/reports/{version_range[0].replace(".","_")}___{version_range[1].replace(".","_")}.html"
    report_dir = f"{result_paths['results_base_folder']}/reports"
    if not os.path.isdir(report_dir):
        os.makedirs(report_dir)
    with open(final_html_filename, "w") as f:
        f.write(result_html)
    return final_html_filename

def collect_and_generate_test_results(version:str, result_paths:dict):
    valid_result_folders        = get_valid_result_folders(version, result_paths)
    version_range               = get_version_range(valid_result_folders)
    sorted_valid_result_folders = sort_valid_result_folders_new_to_old(valid_result_folders)
    regression_test_names       = get_all_regression_tests_by_name(result_paths, valid_result_folders)
    regression_test_files       = collect_experiment_filenames_per_experiment(regression_test_names, result_paths, sorted_valid_result_folders)
    regression_test_summaries   = collect_experiment_summaries(version_range, result_paths, sorted_valid_result_folders, regression_test_names, regression_test_files)
    final_html_file = render_regression_test_result_html(version_range, result_paths, regression_test_summaries)
    webbrowser.open(final_html_file)
    return

def run_experiments(experiments):
    experiments = Profiler.RunExperiments(experiments)

def add_exp(exps:list, name, cmd, exp_fldr, env=None, cwd=None, log_fn=None, bin_fn=None, file_comparison:tuple=None, store_results=True) -> list:
    exps.append(Profiler.Experiment(name=name, command=cmd, experiment_folder=exp_fldr, environment_variables=env, cwd=cwd, geodms_logfile=log_fn, binary_experiment_file=bin_fn, file_comparison=file_comparison, store_results=store_results))
    return exps

def add_cexp(exps:list, name, cmd, exp_fldr, env=None, cwd=None, log_fn=None, bin_fn=None, file_comparison:tuple=None) -> list:
    exps.append(Profiler.Experiment(name=name, command=cmd, experiment_folder=exp_fldr, environment_variables=env, cwd=cwd, geodms_logfile=log_fn, binary_experiment_file=bin_fn, file_comparison=file_comparison))
    return exps