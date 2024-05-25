#!/usr/bin/env python

import argparse
import json

def merge_json_files(program_file, program_input_file):
    try:
        # Open and load the JSON files
        with open(program_file, 'r') as pf, open(program_input_file, 'r') as pif:
            program_data = json.load(pf)
            program_input_data = json.load(pif)
        
        # Merge the JSON data with specified keys
        merged_data = {
            "program": program_data,
            "program_input": program_input_data,
        }
        
        # Print the merged JSON data to stdout
        return json.dumps(merged_data, indent=4)
    
    except Exception as e:
        return f"An error occurred: {e}"

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Merge two JSON files with different keys.")
    parser.add_argument('program_file', type=str, help='Program JSON file')
    parser.add_argument('program_input_file', type=str, help='Program Input JSON file')

    args = parser.parse_args()

    print(merge_json_files(args.program_file, args.program_input_file))
