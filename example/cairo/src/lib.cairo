mod utils;

fn main(n: felt252) -> Array<felt252> {
    array![utils::factorial(n)]
}


#[cfg(test)]
mod tests {
    use super::utils::fib;

    #[test]
    fn it_works() {
        assert(fib(16) == 987, 'it works!');
    }
}
