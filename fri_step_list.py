import math
import sys
import json

# python3 fri_step_list.py ../fibonacci_public_input.json ./new_cpu_air_params.json
if len(sys.argv) != 3:
    raise ValueError('Please provide the path to the public input file and the destination file for new cpu air '
                     'params json file')
public_input_file_path = sys.argv[1]
cpu_air_params_destination_path = sys.argv[2]

with open(public_input_file_path, 'r') as file:
    public_input_file = json.load(file)
    program_n_steps = public_input_file['n_steps']

print(program_n_steps)
last_layer_degree_bound = 128
last_layer_degree_bound_log = math.ceil(
    math.log(last_layer_degree_bound, 2)
)
program_n_steps_log = math.ceil(math.log(program_n_steps, 2))
sigma_fri_step_list = program_n_steps_log + 4 - last_layer_degree_bound_log

(q, r) = divmod(sigma_fri_step_list, 4)
fri_step_list = [4] * q
if r > 0:
    fri_step_list.append(r)

with open('e2e_test/Cairo/cpu_air_params.json', 'r') as file:
    cpu_air_params = json.load(file)
    cpu_air_params['stark']['fri']['fri_step_list'] = fri_step_list
    with open(cpu_air_params_destination_path, 'w') as new_file:
        json.dump(cpu_air_params, new_file, indent=4)