#!/usr/bin/env python3

import json
import math
import sys
from typing import Dict, List

TEMPLATE = {
  "field": "PrimeField0",
  "channel_hash": "poseidon3",
  "commitment_hash": "blake256_masked160_lsb",
  "n_verifier_friendly_commitment_layers": 9999,
  "pow_hash": "keccak256",
  "statement": {
    "page_hash": "pedersen"
  },
  "stark": {
    "fri": {
      "fri_step_list": [0, 4, 4, 4],
      "last_layer_degree_bound": 128,
      "n_queries": 16,
      "proof_of_work_bits": 30
    },
    "log_n_cosets": 3
  },
  "use_extension_field": False,
  "verifier_friendly_channel_updates": True,
  "verifier_friendly_commitment_hash": "poseidon3"
}

def read_json_from_stdin() -> Dict:
    """Read and return the JSON data from standard input."""
    try:
        return json.load(sys.stdin)
    except json.JSONDecodeError:
        print("Error: Invalid JSON input.")
        raise

def calculate_fri_step_list(n_steps: int, degree_bound: int) -> List[int]:
    """Calculate the FRI step list based on the number of steps and the degree bound."""
    fri_degree = round(math.log(n_steps / degree_bound, 2)) + 4
    return [0, *[4 for _ in range(fri_degree // 4)], fri_degree % 4]

def update_template_and_output(template: Dict, fri_step_list: List[int]):
    """Update the template with the new FRI step list and output to standard output."""
    template["stark"]["fri"]["fri_step_list"] = fri_step_list
    json.dump(template, sys.stdout, indent=2)

def main():
    program_public_input = read_json_from_stdin()
    n_steps = int(program_public_input["n_steps"])
    last_layer_degree_bound = int(TEMPLATE["stark"]["fri"]["last_layer_degree_bound"])

    fri_step_list = calculate_fri_step_list(n_steps, last_layer_degree_bound)
    update_template_and_output(TEMPLATE, fri_step_list)

if __name__ == "__main__":
    main()
