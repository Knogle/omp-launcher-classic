#![cfg_attr(not(target_os = "windows"), allow(dead_code, unused_imports))]

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// This file is a modified derivative of code from:
// https://github.com/openmultiplayer/launcher
//
// Upstream reference files:
// - src-tauri/src/injector.rs
// - src-tauri/src/main.rs
// - src-tauri/src/constants.rs
//
// This project adapts that launch and injection flow into a standalone
// helper binary used by omp-launcher-classic.

use std::env;
use std::fmt;
#[cfg(target_os = "windows")]
use std::path::PathBuf;
#[cfg(target_os = "windows")]
use std::process::{Command, Stdio};

const INJECTION_MAX_RETRIES: u32 = 5;
const INJECTION_RETRY_DELAY_MS: u64 = 500;
const VORBIS_WAIT_MAX_RETRIES: u32 = 40;
const PROCESS_MODULE_BUFFER_SIZE: usize = 1024;
const GTA_SA_EXECUTABLE: &str = "gta_sa.exe";
const HELPER_PASSWORD_ENV_VAR: &str = "OMP_LAUNCHER_SERVER_PASSWORD";

const ERROR_ACCESS_DENIED: i32 = 5;
const ERROR_ELEVATION_REQUIRED: i32 = 740;

#[derive(Debug)]
enum LauncherError {
    Io(std::io::Error),
    Process(String),
    Injection(String),
    InvalidInput(String),
    AccessDenied(String),
}

impl fmt::Display for LauncherError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LauncherError::Io(err) => write!(f, "IO error: {}", err),
            LauncherError::Process(msg) => write!(f, "Process error: {}", msg),
            LauncherError::Injection(msg) => write!(f, "Injection error: {}", msg),
            LauncherError::InvalidInput(msg) => write!(f, "Invalid input: {}", msg),
            LauncherError::AccessDenied(msg) => write!(f, "Access denied: {}", msg),
        }
    }
}

impl From<std::io::Error> for LauncherError {
    fn from(err: std::io::Error) -> Self {
        match err.raw_os_error() {
            Some(ERROR_ACCESS_DENIED) => LauncherError::AccessDenied("Access denied".to_string()),
            Some(ERROR_ELEVATION_REQUIRED) => {
                LauncherError::AccessDenied("Administrator privileges required".to_string())
            }
            _ => LauncherError::Io(err),
        }
    }
}

type Result<T> = std::result::Result<T, LauncherError>;

#[derive(Default)]
struct CliArgs {
    name: String,
    host: String,
    port: i32,
    gamepath: String,
    dll: String,
    omp_file: String,
    game_exe: String,
}

fn main() {
    match run() {
        Ok(()) => {}
        Err(err) => {
            eprintln!("{}", err);
            std::process::exit(1);
        }
    }
}

fn run() -> Result<()> {
    #[cfg(not(target_os = "windows"))]
    {
        Err(LauncherError::Process(
            "This helper only supports Windows".to_string(),
        ))
    }

    #[cfg(target_os = "windows")]
    {
        let args = parse_args()?;
        run_samp(
            &args.name,
            &args.host,
            args.port,
            &args.gamepath,
            &args.dll,
            &args.omp_file,
            &args.game_exe,
        )
    }
}

fn parse_args() -> Result<CliArgs> {
    let mut parsed = CliArgs::default();
    let mut iter = env::args().skip(1);

    while let Some(arg) = iter.next() {
        let value = iter.next().ok_or_else(|| {
            LauncherError::InvalidInput(format!("Missing value for argument {}", arg))
        })?;
        match arg.as_str() {
            "--name" => parsed.name = value,
            "--host" => parsed.host = value,
            "--port" => {
                parsed.port = value
                    .parse::<i32>()
                    .map_err(|_| LauncherError::InvalidInput(format!("Invalid port {}", value)))?
            }
            "--gamepath" => parsed.gamepath = value,
            "--dll" => parsed.dll = value,
            "--omp-file" => parsed.omp_file = value,
            "--game-exe" => parsed.game_exe = value,
            _ => {
                return Err(LauncherError::InvalidInput(format!(
                    "Unknown argument {}",
                    arg
                )))
            }
        }
    }

    if parsed.name.is_empty()
        || parsed.host.is_empty()
        || parsed.port <= 0
        || parsed.gamepath.is_empty()
        || parsed.dll.is_empty()
        || parsed.omp_file.is_empty()
    {
        return Err(LauncherError::InvalidInput(
            "Required arguments: --name --host --port --gamepath --dll --omp-file".to_string(),
        ));
    }

    Ok(parsed)
}

#[cfg(target_os = "windows")]
fn run_samp(
    name: &str,
    ip: &str,
    port: i32,
    executable_dir: &str,
    dll_path: &str,
    omp_file: &str,
    custom_game_exe: &str,
) -> Result<()> {
    let password = env::var(HELPER_PASSWORD_ENV_VAR).unwrap_or_default();
    let target_game_exe = if custom_game_exe.is_empty() {
        GTA_SA_EXECUTABLE.to_string()
    } else {
        custom_game_exe.to_string()
    };

    let exe_path = PathBuf::from(executable_dir).join(&target_game_exe);
    let exe_path = exe_path.canonicalize().map_err(|e| {
        LauncherError::Process(format!("Invalid executable path {:?}: {}", exe_path, e))
    })?;

    let mut cmd = Command::new(&exe_path);
    let mut ready = cmd
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .env_remove(HELPER_PASSWORD_ENV_VAR)
        .current_dir(executable_dir)
        .arg("-c")
        .arg("-n")
        .arg(name)
        .arg("-h")
        .arg(ip)
        .arg("-p")
        .arg(port.to_string());

    if !password.is_empty() {
        ready = ready.arg("-z").arg(&password);
    }

    match ready.current_dir(executable_dir).spawn() {
        Ok(child) => {
            inject_dll(child.id(), dll_path, 0, false)?;
            inject_dll(child.id(), omp_file, 0, false)?;
            Ok(())
        }
        Err(err) => match err.raw_os_error() {
            Some(ERROR_ELEVATION_REQUIRED) | Some(ERROR_ACCESS_DENIED) => Err(
                LauncherError::AccessDenied("Unable to open game process".to_string()),
            ),
            _ => Err(LauncherError::Process(format!(
                "Failed to spawn process: {}",
                err
            ))),
        },
    }
}

#[cfg(target_os = "windows")]
fn inject_dll(child: u32, dll_path: &str, times: u32, waiting_for_vorbis: bool) -> Result<()> {
    use std::ffi::CStr;
    use std::mem::{size_of, size_of_val};
    use std::os::windows::ffi::OsStrExt;
    use std::thread::sleep;
    use std::time::Duration;
    use winapi::{
        shared::minwindef::{FALSE, HMODULE},
        um::{
            handleapi::CloseHandle,
            libloaderapi::{GetModuleHandleA, GetProcAddress},
            memoryapi::{VirtualAllocEx, VirtualFreeEx, WriteProcessMemory},
            processthreadsapi::{CreateRemoteThread, GetExitCodeThread, OpenProcess},
            psapi::{EnumProcessModulesEx, GetModuleFileNameExA},
            synchapi::WaitForSingleObject,
            winbase::WAIT_OBJECT_0,
            winnt::{PROCESS_QUERY_INFORMATION, PROCESS_VM_READ},
        },
    };

    let inject_once = || -> Result<()> {
        let canonical_dll_path = PathBuf::from(dll_path).canonicalize().map_err(|err| {
            LauncherError::Injection(format!("Invalid DLL path {:?}: {}", dll_path, err))
        })?;
        let wide_dll_path: Vec<u16> = canonical_dll_path
            .as_os_str()
            .encode_wide()
            .chain(std::iter::once(0))
            .collect();
        let bytes = wide_dll_path.len() * size_of::<u16>();

        unsafe {
            let process = OpenProcess(
                winapi::um::winnt::PROCESS_CREATE_THREAD
                    | winapi::um::winnt::PROCESS_QUERY_INFORMATION
                    | winapi::um::winnt::PROCESS_VM_OPERATION
                    | winapi::um::winnt::PROCESS_VM_WRITE
                    | winapi::um::winnt::PROCESS_VM_READ,
                FALSE,
                child,
            );
            if process.is_null() {
                return match std::io::Error::last_os_error().raw_os_error() {
                    Some(ERROR_ELEVATION_REQUIRED) | Some(ERROR_ACCESS_DENIED) => Err(
                        LauncherError::AccessDenied("Unable to open game process".to_string()),
                    ),
                    _ => Err(LauncherError::Process(
                        "Failed to access process for DLL injection".to_string(),
                    )),
                };
            }

            let remote_memory = VirtualAllocEx(
                process,
                std::ptr::null_mut(),
                bytes,
                winapi::um::winnt::MEM_COMMIT | winapi::um::winnt::MEM_RESERVE,
                winapi::um::winnt::PAGE_READWRITE,
            );
            if remote_memory.is_null() {
                CloseHandle(process);
                return Err(LauncherError::Injection(format!(
                    "VirtualAllocEx failed: {}",
                    std::io::Error::last_os_error()
                )));
            }

            let wrote_memory = WriteProcessMemory(
                process,
                remote_memory,
                wide_dll_path.as_ptr().cast(),
                bytes,
                std::ptr::null_mut(),
            );
            if wrote_memory == 0 {
                VirtualFreeEx(process, remote_memory, 0, winapi::um::winnt::MEM_RELEASE);
                CloseHandle(process);
                return Err(LauncherError::Injection(format!(
                    "WriteProcessMemory failed: {}",
                    std::io::Error::last_os_error()
                )));
            }

            let kernel32 = GetModuleHandleA(c"kernel32.dll".as_ptr());
            if kernel32.is_null() {
                VirtualFreeEx(process, remote_memory, 0, winapi::um::winnt::MEM_RELEASE);
                CloseHandle(process);
                return Err(LauncherError::Injection(
                    "Could not locate kernel32.dll".to_string(),
                ));
            }

            let load_library = GetProcAddress(kernel32, c"LoadLibraryW".as_ptr());
            if load_library.is_null() {
                VirtualFreeEx(process, remote_memory, 0, winapi::um::winnt::MEM_RELEASE);
                CloseHandle(process);
                return Err(LauncherError::Injection(
                    "Could not locate LoadLibraryW".to_string(),
                ));
            }

            let thread = CreateRemoteThread(
                process,
                std::ptr::null_mut(),
                0,
                Some(std::mem::transmute(load_library)),
                remote_memory,
                0,
                std::ptr::null_mut(),
            );
            if thread.is_null() {
                VirtualFreeEx(process, remote_memory, 0, winapi::um::winnt::MEM_RELEASE);
                CloseHandle(process);
                return Err(LauncherError::Injection(format!(
                    "CreateRemoteThread failed: {}",
                    std::io::Error::last_os_error()
                )));
            }

            let wait_result = WaitForSingleObject(thread, 5000);
            let mut exit_code = 0;
            let got_exit_code = GetExitCodeThread(thread, &mut exit_code);

            CloseHandle(thread);
            VirtualFreeEx(process, remote_memory, 0, winapi::um::winnt::MEM_RELEASE);
            CloseHandle(process);

            if wait_result != WAIT_OBJECT_0 {
                return Err(LauncherError::Injection(
                    "Timed out waiting for remote LoadLibraryW".to_string(),
                ));
            }

            if got_exit_code == 0 {
                return Err(LauncherError::Injection(format!(
                    "GetExitCodeThread failed: {}",
                    std::io::Error::last_os_error()
                )));
            }

            if exit_code == 0 {
                return Err(LauncherError::Injection(format!(
                    "LoadLibraryW failed for {}",
                    canonical_dll_path.display()
                )));
            }
        }

        Ok(())
    };

    let wait_for_vorbis_modules = || -> Result<()> {
        for _ in 0..=VORBIS_WAIT_MAX_RETRIES {
            unsafe {
                let handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, child);
                if handle.is_null() {
                    return Err(LauncherError::Process(
                        "Failed to open process while waiting for vorbis".to_string(),
                    ));
                }

                let mut module_handles: [HMODULE; PROCESS_MODULE_BUFFER_SIZE] =
                    [0 as *mut _; PROCESS_MODULE_BUFFER_SIZE];
                let mut found = 0;

                EnumProcessModulesEx(
                    handle,
                    module_handles.as_mut_ptr(),
                    size_of_val(&module_handles) as _,
                    &mut found,
                    0x03,
                );

                let mut found_vorbis = false;
                let mut bytes = [0i8; PROCESS_MODULE_BUFFER_SIZE];
                if found != 0 {
                    for i in 0..(found / size_of::<HMODULE>() as u32) {
                        if GetModuleFileNameExA(
                            handle,
                            module_handles[i as usize],
                            bytes.as_mut_ptr(),
                            PROCESS_MODULE_BUFFER_SIZE as u32,
                        ) != 0
                        {
                            let module_name = CStr::from_ptr(bytes.as_ptr()).to_string_lossy();
                            if module_name.contains("vorbis") {
                                found_vorbis = true;
                                break;
                            }
                        }
                    }
                }

                CloseHandle(handle);

                if found_vorbis {
                    return Ok(());
                }
            }

            sleep(Duration::from_millis(INJECTION_RETRY_DELAY_MS));
        }

        Err(LauncherError::Injection(
            "Timed out waiting for vorbis modules before DLL injection".to_string(),
        ))
    };

    let mut attempt = times;
    let mut wait_for_vorbis_phase = waiting_for_vorbis;

    loop {
        if wait_for_vorbis_phase {
            wait_for_vorbis_modules()?;
        }

        match inject_once() {
            Ok(()) => return Ok(()),
            Err(err) => {
                sleep(Duration::from_millis(INJECTION_RETRY_DELAY_MS));
                if attempt >= INJECTION_MAX_RETRIES {
                    if !wait_for_vorbis_phase {
                        wait_for_vorbis_phase = true;
                        attempt = 0;
                        continue;
                    }
                    return Err(err);
                }

                attempt += 1;
            }
        }
    }
}
