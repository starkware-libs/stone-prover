use core::felt252;

fn main() -> Array<felt252> {
    let n = 10;
    let mut result = array![];
    result.append(fib(1, 1, n));
    result
}

fn fib(a: felt252, b: felt252, n: felt252) -> felt252 {
    match n {
        0 => a,
        _ => fib(b, a + b, n - 1),
    }
}
