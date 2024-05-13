use core::traits::Into;
fn main(a: felt252, b: u32) -> felt252{
    add_two(a, b.into())
}

fn add_two(a: felt252, b: felt252) -> felt252{
    a + b
}