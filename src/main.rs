#![allow(dead_code)]
use core::fmt::{self, Write};
use std::collections::HashMap;
use std::ffi::{OsStr, OsString};
use std::io::{self, Read as IoRead, Write as IoWrite};
use std::process::{self, Stdio};

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

    let mut watcher = Watcher::new("bash");

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

struct Buffer<const N: usize> {
    buffer: [u8; N],
    cursor: usize,
}

impl<const N: usize> Buffer<N> {
    fn new() -> Self {
        Self {
            buffer: [0; N],
            cursor: 0,
        }
    }

    fn pull(&mut self) -> &[u8] {
        let length = self.cursor;
        self.cursor = 0;
        &self.buffer[0..length]
    }

    fn peek(&self) -> &[u8] {
        &self.buffer[0..self.cursor]
    }

    fn read_from<R>(&mut self, readable: &mut R) -> io::Result<()>
    where
        R: IoRead,
    {
        let bytes = readable.read(&mut self.buffer[self.cursor..])?;
        self.cursor += bytes;
        Ok(())
    }
}

impl<const N: usize> Write for Buffer<N> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        if self.cursor + s.len() <= N {
            let next_cursor = self.cursor + s.len();
            self.buffer[self.cursor..next_cursor].copy_from_slice(s.as_bytes());
            self.cursor = next_cursor;
            Ok(())
        } else {
            Err(fmt::Error)
        }
    }
}

struct ChildProcess {
    child: process::Child,
}

impl Drop for ChildProcess {
    fn drop(&mut self) {
        let _ = self.child.kill();
    }
}

impl ChildProcess {
    fn new(shell: &str) -> Result<Self, io::Error> {
        let child = process::Command::new(shell)
            // don't share stdin/out with this (parent) process:
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()?;
        Ok(Self { child })
    }

    fn add_stdin<const N: usize>(&mut self, buffer: &mut Buffer<N>) {
        self.child
            .stdin
            .as_mut()
            .unwrap()
            .write_all(buffer.pull())
            .expect("couldn't write to child process stdin");
    }

    fn get_stdout<const N: usize>(&mut self, buffer: &mut Buffer<N>) {
        self.child
            .stdin
            .as_mut()
            .unwrap()
            .flush()
            .expect("couldn't flush child process stdin");
        buffer
            .read_from(self.child.stdout.as_mut().unwrap())
            .expect("couldn't read child process stdout");
    }
}

struct Watcher {
    child_process: ChildProcess,
    inotify: Inotify,
    watch_map: HashMap<WatchDescriptor, OsString>,
}

impl Watcher {
    fn new(shell: &str) -> Self {
        Self {
            child_process: ChildProcess::new(shell).expect("failed to spawn a child shell process"),
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

    fn handle_update(&mut self, file_path: OsString, from_directory_update: bool) {
        if from_directory_update {
            log::warn!("got directory update, ignoring, use `fthis {file_path:?}` instead");
            return;
        }
        log::info!("got update from file {file_path:?}");
        self.execute_file(file_path.as_os_str());
        // NOTE: for some reason, individual files need to be re-watched.
        // this works out for us because we don't want `fthis`'s writes
        // (via `self.execute`) to trigger another update before the user
        // makes a change.
        let _ = self.add(file_path);
    }

    fn execute_file(&mut self, file_path: &OsStr) {
        let mut buffer = Buffer::<1024>::new();
        let _ = std::write!(&mut buffer, "echo asdf\n");
        self.child_process.add_stdin(&mut buffer);
        self.child_process.get_stdout(&mut buffer);
        let output = std::str::from_utf8(buffer.pull()).expect("ok");
        log::info!("got output {output}");
    }
}
