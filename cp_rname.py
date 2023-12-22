
import os
import re
import json
import shutil

def copy_files(source_folder, destination_folder):
    file_list = os.listdir(source_folder)

    for file_name in file_list:
        if file_name.startswith("code_contests_train.riegeli-") and file_name.endswith("_perfed.jsonl"):
            source_file = os.path.join(source_folder, file_name)

            no = int(re.findall(r"code_contests_train.riegeli-(\d+)-of-00128_perfed.jsonl", file_name)[0])
            destination_file = os.path.join(destination_folder, str(no) + ".jsonl")

            shutil.copyfile(source_file, destination_file)
            print(f"Copy {file_name} successfully!")


def change_file_name(destination_folder):
    file_list = os.listdir(destination_folder)

    for file_name in file_list:
        if not file_name.endswith(".jsonl"):
            continue
        file_path = os.path.join(destination_folder, file_name)
        with open(file_path, "r") as f:
            first_line = json.loads(f.readline())
            last_line = json.loads(f.readlines()[-1])
            firstid = first_line["id"]
            lastid = last_line["id"]
            new_file_name = f"{firstid}_{lastid+1}.jsonl"
            new_file_path = os.path.join(destination_folder, new_file_name)

        os.rename(file_path, new_file_path)
        print(f"Change {file_name} to {new_file_name} successfully!")



if __name__ == "__main__":
    source_folder = "/coconut/songrun-data/datasets/dm-code_contests/"
    destination_folder = "/coconut/songrun-data/code_contest_data"

    copy_files(source_folder, destination_folder)

    change_file_name(destination_folder)
