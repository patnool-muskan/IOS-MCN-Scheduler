import os
import xml.etree.ElementTree as ET
import time
import subprocess
import sys
import re

XML_DIR = "cicd/xml_files/"

GITHUB_REPOSITORY_OWNER = os.getenv("GITHUB_REPOSITORY_OWNER")
IMAGE_VERSION = os.getenv("IMAGE_VERSION")  # Can default to 'latest' or 'version'

def get_full_image_name(image):
    return f"ghcr.io/{GITHUB_REPOSITORY_OWNER}/{image}:{IMAGE_VERSION}"

def parse_test_cases(xml_file):
    tree = ET.parse(xml_file)
    root = tree.getroot()

    requested_ids_text = root.find('TestCaseRequestedList').text
    requested_ids = [line.strip() for line in requested_ids_text.strip().splitlines() if line.strip()]

    test_case_elements = root.findall('testCase')
    test_cases = {}
    for tc in test_case_elements:
        tc_id = tc.attrib.get('id')
        tc_data = {
            'class': tc.findtext('class'),
            'desc': tc.findtext('desc'),
        }

        for child in tc:
            if child.tag not in ['class', 'desc']:
                value = child.text
                if value and " " in value:
                    tc_data[child.tag] = value.strip().split()
                else:
                    tc_data[child.tag] = value.strip() if value else None

        test_cases[tc_id] = tc_data

    return requested_ids, test_cases

def pull_docker_images(images):
    for image in images:
        full_image = get_full_image_name(image)
        print(f"Pulling Docker image: {full_image}")
        try:
            result = subprocess.run(['docker', 'pull', full_image], check=True, capture_output=True, text=True)
            print(result.stdout)
        except subprocess.CalledProcessError as e:
            print(f"Failed to pull image {full_image}: {e.stderr}")

def deploy_docker_compose(yaml_path):
    print(f"Deploying services using docker-compose at: {yaml_path}")
    if not os.path.exists(yaml_path):
        print(f"YAML file not found: {yaml_path}")
        return False

    try:
        result = subprocess.run(['docker-compose', '-f', yaml_path, 'up', '-d'], check=True, capture_output=True, text=True)
        print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Failed to deploy using {yaml_path}:\n{e.stderr}")
        return False

def undeploy_docker_compose(yaml_path):
    print(f"Undeploying services using docker-compose at: {yaml_path}")
    if not os.path.exists(yaml_path):
        print(f"YAML file not found: {yaml_path}")
        return False

    try:
        result = subprocess.run(['docker-compose', '-f', yaml_path, 'down'], check=True, capture_output=True, text=True)
        print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Failed to undeploy using {yaml_path}:\n{e.stderr}")
        return False

def get_ue_ip(container_name, iface='oaitun_ue1', timeout=30):
    """
    Return the IPv4 address (no CIDR) for iface inside container_name, or None if not found within timeout.
    """
    start = time.time()
    while time.time() - start < timeout:
        try:
            res = subprocess.run(
                ['docker', 'exec', container_name, 'ip', '-4', 'addr', 'show', 'dev', iface],
                capture_output=True, text=True, check=True
            )
            out = res.stdout or ""
            for line in out.splitlines():
                if 'inet ' in line:
                    ip_cidr = line.strip().split()[1]
                    ip = ip_cidr.split('/')[0]
                    return ip
        except subprocess.CalledProcessError:
            # container not ready or interface not present yet
            pass
        time.sleep(2)
    return None

def check_ue_ip_assigned(container_name, timeout=30):
    """Return True if UE container has an IP on the expected interface within timeout."""
    print(f"Checking IP assignment for UE container: {container_name}")
    ip = get_ue_ip(container_name, timeout=timeout)
    if ip:
        print(f"IP assigned to {container_name}: {ip}")
        return True
    print(f"IP not assigned to {container_name} within {timeout} seconds.")
    return False

def clean_docker_images(image_list):
    print(f"Cleaning Docker images: {image_list}")
    for image in image_list:
        full_image = get_full_image_name(image)
        try:
            result = subprocess.run(['docker', 'rmi', '-f', full_image], check=True, capture_output=True, text=True)
            print(f"Removed image: {image}")
        except subprocess.CalledProcessError as e:
            if "No such image" in e.stderr:
                print(f"Image not found: {full_image}")
            else:
                print(f"Error removing image {full_image}:\n{e.stderr}")

def run_test_case(test_id, data):
    print(f"\n--- Running Test Case {test_id} ---")
    print(f"Class      : {data.get('class')}")
    print(f"Description: {data.get('desc')}")

    try:
        if data.get("class") == "Pull_Local_Registry":
            images = data.get("images", [])
            if isinstance(images, str):
                images = images.split()
            pull_docker_images(images)

        elif data.get("class") == "Deploy_Object":
            yaml_path = data.get("yaml_path")
            if yaml_path:
                return deploy_docker_compose(yaml_path)
            else:
                print("No YAML path provided for deployment.")
                return False

        elif data.get("class") == "Undeploy_Object":
            yaml_path = data.get("yaml_path")
            if yaml_path:
                return undeploy_docker_compose(yaml_path)
            else:
                print("No YAML path provided for undeployment.")
                return False

        elif data.get("class") == "Attach_UE":
            ue_ids = data.get("id", [])
            if isinstance(ue_ids, str):
                ue_ids = ue_ids.split()
            for ue_id in ue_ids:
                if not check_ue_ip_assigned(ue_id):
                    return False

        elif data.get("class") == "Ping":
            ue_ids = data.get("id") or []
            if isinstance(ue_ids, str):
                ue_ids = ue_ids.split()
            elif not isinstance(ue_ids, list):
                ue_ids = [str(ue_ids)]

            raw_ping_args = data.get("ping_args") or ""
            if isinstance(raw_ping_args, list):
                ping_args_list = raw_ping_args
            else:
                ping_args_list = raw_ping_args.split() if raw_ping_args.strip() else []

            try:
                loss_threshold = int(data.get("ping_packetloss_threshold") or 0)
            except ValueError:
                loss_threshold = 0

            print(f"Running Ping test (host -> UE IP): UEs={ue_ids}, args={ping_args_list}, threshold={loss_threshold}%")

            for ue in ue_ids:
                print(f"Searching UE IP inside container: {ue}")
                ue_ip = get_ue_ip(ue, timeout=10)
                if not ue_ip:
                    print(f"Ping FAILED for UE {ue}: no IP assigned on interface 'oaitun_ue1' within timeout.")
                    return False

                print(f"UE {ue} IP = {ue_ip} -- executing host ping")
                cmd = ['ping', ue_ip] + ping_args_list
                print("Executing (host):", " ".join(cmd))

                try:
                    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                    output = result.stdout or ""
                    print(output)

                    packet_loss = 0
                    for line in output.splitlines():
                        if "packet loss" in line:
                            m = re.search(r'(\d+)%\s*packet loss', line)
                            if m:
                                packet_loss = int(m.group(1))
                            else:
                                nums = re.findall(r'(\d+)%', line)
                                if nums:
                                    packet_loss = int(nums[0])
                            break

                    print(f"{ue} (IP {ue_ip}): packet loss = {packet_loss}%")

                    if packet_loss > loss_threshold:
                        print(f"Ping FAILED for {ue}: loss {packet_loss}% > threshold {loss_threshold}%")
                        return False
                    else:
                        print(f"Ping PASSED for {ue}")

                except subprocess.CalledProcessError as e:
                    err = e.stderr or e.stdout or str(e)
                    print(f"Ping command failed for {ue} (host -> {ue_ip}): {err}")
                    return False

            return True

        elif data.get("class") == "Clean_Test_Server_Images":
            images = data.get("images", [])
            if isinstance(images, str):
                images = images.split()
            clean_docker_images(images)

        else:
            for key, value in data.items():
                if key not in ['class', 'desc']:
                    print(f"{key}: {value}")

        print(f"Test Case {test_id} completed.")
        return True

    except Exception as e:
        print(f"Test Case {test_id} failed due to error: {e}")
        return False

def main():
    if not os.path.exists(XML_DIR):
        print("No test cases available (directory missing).")
        sys.exit(1)

    xml_files = [f for f in os.listdir(XML_DIR) if f.endswith('.xml')]

    if not xml_files:
        print("No test cases available (no XML files found).")
        sys.exit(1)

    passed_ids = []
    failed_ids = []

    for xml_file in xml_files:
        print(f"\n=== Processing file: {xml_file} ===")
        full_path = os.path.join(XML_DIR, xml_file)
        try:
            requested_ids, test_cases = parse_test_cases(full_path)
            for tc_id in requested_ids:
                tc_data = test_cases.get(tc_id)
                if tc_data:
                    result = run_test_case(tc_id, tc_data)
                    if result:
                        passed_ids.append(tc_id)
                    else:
                        failed_ids.append(tc_id)
                else:
                    print(f"Test case ID {tc_id} not found in file.")
                    failed_ids.append(tc_id)
        except Exception as e:
            print(f"Error processing {xml_file}: {e}")
            failed_ids.append(f"{xml_file}: ALL")

    print("\nTest Summary")
    print(f"Passed ({len(passed_ids)}): {', '.join(passed_ids) if passed_ids else 'None'}")
    print(f"Failed ({len(failed_ids)}): {', '.join(failed_ids) if failed_ids else 'None'}")
    print("All test cases from all XML files processed.")

if __name__ == "__main__":
    main()
