fn main() {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    let mut arg_count = 0;
    let mut paths: Vec<String> = vec![];

    for path in std::env::args() {
        if arg_count > 0 {
            paths.push(path);
        }
        arg_count += 1;
    }

    std::assert!(arg_count > 1, "usage: `fthis [file_path] [directory_path]`");

    for path in paths {
        log::warn!("(not actually) watching {path}");
    }
}
