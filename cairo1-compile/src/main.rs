use std::{
    fmt,
    fs::File,
    io::{BufReader, Write},
    path::{Path, PathBuf},
};

use cairo_lang_compiler::{
    compile_prepared_db, db::RootDatabase, project::setup_project, CompilerConfig,
};
use cairo_lang_sierra::program::Program;
use clap::{Parser, ValueHint};
use serde::Serialize;

#[derive(Parser, Debug)]
struct CompileArgs {
    #[clap(value_parser, value_hint=ValueHint::FilePath, value_name = "FILE")]
    program: PathBuf,
    #[clap(short, long, value_name = "FILE")]
    output: Option<PathBuf>,
}

#[derive(Parser, Debug)]
struct MergeArgs {
    #[clap(value_parser, value_hint=ValueHint::FilePath, value_name = "FILE")]
    sierra: PathBuf,
    #[clap(value_parser, value_hint=ValueHint::FilePath, value_name = "FILE")]
    input: PathBuf,
    #[clap(short, long, value_name = "FILE")]
    output: Option<PathBuf>,
    #[clap(short, long, value_enum)]
    layout: Option<Layout>,
}

#[derive(clap::ValueEnum, Clone, Debug, Default)]
enum Layout {
    #[default]
    Recursive,
}

#[derive(Parser, Debug)]
#[clap(author, version, about, long_about = None)]
enum Args {
    Compile(CompileArgs),
    Merge(MergeArgs),
}

fn main() {
    match Args::parse() {
        Args::Compile(args) => compile(args),
        Args::Merge(args) => merge(args),
    }
}

#[derive(Debug, Clone, Serialize)]
struct ProgramWithArgs {
    program: Program,
    program_input: serde_json::Value,
    layout: String,
}

fn merge(args: MergeArgs) {
    let sierra_program: Program =
        serde_json::from_reader(BufReader::new(File::open(args.sierra).unwrap())).unwrap();
    let input: serde_json::Value =
        serde_json::from_reader(BufReader::new(File::open(args.input).unwrap())).unwrap();

    let layout = args.layout.unwrap_or(Layout::Recursive);
    let merged = serde_json::to_string(&ProgramWithArgs {
        program: sierra_program,
        program_input: input,
        layout: layout.to_string(),
    })
    .unwrap();
    match args.output {
        Some(output) => {
            let mut file = std::fs::File::create(output).unwrap();
            file.write_all(merged.as_bytes()).unwrap();
        }
        None => {
            println!("{}", merged)
        }
    }
}

fn compile(args: CompileArgs) {
    let program = compile_sierra(&args.program);
    let json_program = serde_json::to_string(&program).unwrap();
    match args.output {
        Some(output) => {
            let mut file = std::fs::File::create(output).unwrap();
            file.write_all(json_program.as_bytes()).unwrap();
        }
        None => {
            println!("{}", json_program)
        }
    }
}

fn compile_sierra(filename: &Path) -> Program {
    let compiler_config = CompilerConfig {
        replace_ids: true,
        ..CompilerConfig::default()
    };
    let mut db = RootDatabase::builder()
        .detect_corelib()
        .skip_auto_withdraw_gas()
        .build()
        .unwrap();
    let main_crate_ids = setup_project(&mut db, filename).unwrap();
    compile_prepared_db(&mut db, main_crate_ids, compiler_config).unwrap().program
}

impl fmt::Display for Layout {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Layout::Recursive => write!(f, "recursive"),
        }
    }
}
