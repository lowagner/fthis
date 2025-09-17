use std::collections::HashMap;
use std::ffi::{OsStr, OsString};

use inotify::{Event, Inotify, WatchDescriptor, WatchMask};

fn main() {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    let mut arg_count = 0;
    let mut paths: Vec<OsString> = vec![];

    for path in std::env::args() {
        if arg_count > 0 {
            paths.push(path.into());
        }
        arg_count += 1;
    }
    std::assert!(arg_count > 1, "usage: `fthis [file_path] [directory_path]`");

    let mut watcher = Watcher::init();

    let mut success_count = 0;
    for path in paths {
        if watcher.add(path).is_ok() {
            success_count += 1;
        }
    }
    std::assert!(success_count > 0, "all files failed to be watched");

    loop {
        log::info!("watching for changes...");
        watcher.watch();
    }
}

struct Watcher {
    inotify: Inotify,
    watch_map: HashMap<WatchDescriptor, OsString>,
}

impl Watcher {
    fn init() -> Self {
        Self {
            inotify: Inotify::init().expect("error initializing inotify instance"),
            watch_map: HashMap::new(),
        }
    }

    fn add(&mut self, path: OsString) -> Result<(), std::io::Error> {
        match self
            .inotify
            .watches()
            .add(path.as_os_str(), WatchMask::MODIFY)
        {
            Ok(watch) => {
                log::info!("watching {path:?}");
                self.watch_map.insert(watch, path);
                Ok(())
            }
            Err(err) => {
                log::warn!("could not watch {path:?}, error: {err}");
                Err(err)
            }
        }
    }

    fn watch(&mut self) {
        let mut buffer = [0; 1024];
        let events = self
            .inotify
            .read_events_blocking(&mut buffer)
            .expect("error while reading events");

        for event in events {
            let file_path = self.file_path(&event);
            if file_path.len() > 0 {
                self.handle_update(file_path, event.name.is_some());
            }
        }
    }

    fn file_path(&self, event: &Event<&OsStr>) -> OsString {
        // Directories have the file-path in `event.name`:
        if let Some(name) = event.name {
            return name.to_os_string();
        }
        // Files need to be retrieved via the `WatchDescriptor`:
        if let Some(name) = self.watch_map.get(&event.wd) {
            return name.clone();
        }
        return "".into();
    }

    fn handle_update(&mut self, file_path: OsString, directory_update: bool) {
        log::info!("got event from file {file_path:?}");
        if !directory_update {
            // NOTE: for some reason, individual files need to be re-watched.
            let _ = self.add(file_path);
        }
    }
}
