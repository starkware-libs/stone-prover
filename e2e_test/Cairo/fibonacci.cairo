use core::felt252;

fn main(n: felt252) -> felt252 {
    let result = fib(1, 1, n);
    result
}

fn fib(a: felt252, b: felt252, n: felt252) -> felt252 {
    match n {
        0 => a,
        _ => fib(b, a + b, n - 1),
    }
}
