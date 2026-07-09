import xml.etree.ElementTree as ET
import subprocess
import os
import time
import signal
import argparse

TIMEOUT = 300  # Set timeout in seconds (5 minutes)

def start_process(command, log_file):
    with open(log_file, "w") as log:
        process = subprocess.Popen(command, shell=True, stdout=log, stderr=log, preexec_fn=os.setsid)
    return process

def monitor_log(log_file, success_message, timeout=TIMEOUT):
    print(f"Checking {log_file} Logs")

    start_time = time.time()

    while True:
        elapsed_time = time.time() - start_time
        if elapsed_time > timeout:
            print("Test failed: UE Connection Failed.")
            stop_all_processes()
            exit(1)

        try:
            with open(log_file, "r") as log:
                log.seek(0, os.SEEK_END)
                while True:
                    if time.time() - start_time > timeout:
                        print("Test failed: UE Connection Failed.")
                        stop_all_processes()
                        exit(1)

                    line = log.readline()
                    if not line:
                        time.sleep(0.5)
                        continue

                    if success_message in line:
                        print("UE Attached Successfully")
                        return
        except FileNotFoundError:
            print(f"Log file {log_file} not found yet, retrying...")
            time.sleep(1)

def stop_all_processes():
    print("Stopping all processes...")
    for process in processes.values():
        os.killpg(os.getpgid(process.pid), signal.SIGTERM)
    print("All processes stopped.")


parser = argparse.ArgumentParser(description="Run a specific test case from test_cases.xml")
parser.add_argument("--testcase", type=str, required=True, help="Name of the test case to run")
args = parser.parse_args()

tree = ET.parse("xml_files/baremetal_xml_files/test_cases.xml")
root = tree.getroot()

testcases = {testcase.get("name"): testcase for testcase in root.findall("testcase")}

if args.testcase not in testcases:
    print(f"Error: Test case '{args.testcase}' not found in test_cases.xml.")
    exit(1)

selected_testcase = testcases[args.testcase]

commands = [cmd.text for cmd in selected_testcase.findall("command")]
logfiles = [log.text for log in selected_testcase.findall("logfile")]
success_message = selected_testcase.find("result").text.strip()

if len(commands) != len(logfiles):
    print(f"Error: Mismatch between commands ({len(commands)}) and log files ({len(logfiles)}) in test case '{args.testcase}'.")
    exit(1)

processes = {}
print(f"Running test case: {args.testcase}")

cucp_index = logfiles.index("../log/rfsim/gnb-cucp.sa.f1-e1.iisc.conf.log")
print(f"Starting CUCP: {commands[cucp_index]}")
processes["cucp"] = start_process(commands[cucp_index], logfiles[cucp_index])

cuup_index = logfiles.index("../log/rfsim/gnb-cuup.sa.f1-e1.iisc.conf.log")
print(f"Starting CUUP: {commands[cuup_index]}")
processes["cuup"] = start_process(commands[cuup_index], logfiles[cuup_index])

if args.testcase == "CU + DU":
    du1_index = logfiles.index("../log/rfsim/gnb-du.sa.band78.106PRB.1x1-f1-e1-usrpx310.conf.log")
    ue1_index = logfiles.index("../log/rfsim/ue-1.log")

    print(f"Starting DU-1: {commands[du1_index]}")
    processes["du-1"] = start_process(commands[du1_index], logfiles[du1_index])

    print(f"Starting UE-1: {commands[ue1_index]}")
    processes["ue-1"] = start_process(commands[ue1_index], logfiles[ue1_index])

    monitor_log("../log/rfsim/gnb-du.sa.band78.106PRB.1x1-f1-e1-usrpx310.conf.log", success_message)

elif args.testcase == "CU + 2-DU's":
    du1_index = logfiles.index("../log/rfsim/gnb-du.sa.band78.106PRB.1x1-f1-e1-usrpx310.conf.log")
    ue1_index = logfiles.index("../log/rfsim/ue-1.log")

    print(f"Starting DU-1: {commands[du1_index]}")
    processes["du-1"] = start_process(commands[du1_index], logfiles[du1_index])

    print(f"Starting UE-1: {commands[ue1_index]}")
    processes["ue-1"] = start_process(commands[ue1_index], logfiles[ue1_index])

    monitor_log("../log/rfsim/gnb-du.sa.band78.106PRB.1x1-f1-e1-usrpx310.conf.log", success_message)

    du2_index = logfiles.index("../log/rfsim/gnb-du-1.sa.band78.106PRB.1x1-f1-e1-usrpx310.conf.log")
    ue2_index = logfiles.index("../log/rfsim/ue-2.log")

    print(f"Starting DU-2: {commands[du2_index]}")
    processes["du-2"] = start_process(commands[du2_index], logfiles[du2_index])

    print(f"Starting UE-2: {commands[ue2_index]}")
    processes["ue-2"] = start_process(commands[ue2_index], logfiles[ue2_index])

    monitor_log("../log/rfsim/gnb-du-1.sa.band78.106PRB.1x1-f1-e1-usrpx310.conf.log", success_message)

else:
    print(f"No DU logs found for monitoring in test case '{args.testcase}'.")
    stop_all_processes()
    exit(1)

print(f"Testcase: '{args.testcase}' Passed Successfully!")
stop_all_processes()
exit(0)
