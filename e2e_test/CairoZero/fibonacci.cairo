// Copyright 2023 StarkWare Industries Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License").
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.starkware.co/open-source-license/
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.

%builtins output
func main(output_ptr: felt*) -> (output_ptr: felt*) {
    alloc_locals;

    // Load fibonacci_claim_index and copy it to the output segment.
    local fibonacci_claim_index;
    %{ ids.fibonacci_claim_index = program_input['fibonacci_claim_index'] %}

    assert output_ptr[0] = fibonacci_claim_index;
    let res = fib(1, 1, fibonacci_claim_index);
    assert output_ptr[1] = res;

    // Return the updated output_ptr.
    return (output_ptr=&output_ptr[2]);
}

func fib(first_element: felt, second_element: felt, n: felt) -> felt {
    if (n == 0) {
        return second_element;
    }

    return fib(
        first_element=second_element, second_element=first_element + second_element, n=n - 1
    );
}
