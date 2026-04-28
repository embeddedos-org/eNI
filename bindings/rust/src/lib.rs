// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
//! Rust bindings for the eNI embedded Neural Interface library.

use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::c_char;

// ---------------------------------------------------------------------------
// FFI type definitions
// ---------------------------------------------------------------------------

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EniStatus {
    Ok = 0,
    ErrNomem,
    ErrInvalid,
    ErrNotFound,
    ErrIo,
    ErrTimeout,
    ErrPermission,
    ErrRuntime,
    ErrConnect,
    ErrProtocol,
    ErrConfig,
    ErrUnsupported,
    ErrPolicyDenied,
    ErrProvider,
    ErrOverflow,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EniVariant {
    Min = 0,
    Framework,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EniMode {
    Intent = 0,
    Features,
    Raw,
    FeaturesIntent,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EniEventType {
    Intent = 0,
    Features,
    Raw,
    Control,
    Feedback,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EniSessionState {
    Idle = 0,
    Calibrating,
    Running,
    Paused,
    Stopped,
    Error,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EniTimestamp {
    pub sec: u64,
    pub nsec: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct EniIntent {
    pub name: [c_char; 64],
    pub confidence: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct EniEvent {
    pub id: u32,
    pub version: u32,
    pub event_type: EniEventType,
    pub timestamp: EniTimestamp,
    pub source: [c_char; 64],
    pub payload: [u8; 4096],
}

// Opaque structs for C types — we only interact through pointers.
#[repr(C)]
pub struct EniConfig {
    _opaque: [u8; 4096],
}

#[repr(C)]
pub struct EniSession {
    _opaque: [u8; 4096],
}

#[repr(C)]
pub struct EniDspFftCtx {
    _opaque: [u8; 2112],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EniDspFeatures {
    pub band_power: [f32; 5],
    pub total_power: f32,
    pub spectral_entropy: f32,
    pub hjorth_activity: f32,
    pub hjorth_mobility: f32,
    pub hjorth_complexity: f32,
}

// ---------------------------------------------------------------------------
// FFI declarations
// ---------------------------------------------------------------------------

extern "C" {
    // types
    pub fn eni_status_str(status: EniStatus) -> *const c_char;
    pub fn eni_timestamp_now() -> EniTimestamp;

    // config
    pub fn eni_config_init(cfg: *mut EniConfig) -> EniStatus;
    pub fn eni_config_load_file(cfg: *mut EniConfig, path: *const c_char) -> EniStatus;
    pub fn eni_config_load_defaults(cfg: *mut EniConfig, variant: EniVariant) -> EniStatus;

    // event
    pub fn eni_event_init(
        ev: *mut EniEvent,
        event_type: EniEventType,
        source: *const c_char,
    ) -> EniStatus;
    pub fn eni_event_set_intent(
        ev: *mut EniEvent,
        intent: *const c_char,
        confidence: f32,
    ) -> EniStatus;

    // session
    pub fn eni_session_init(session: *mut EniSession) -> EniStatus;
    pub fn eni_session_start(session: *mut EniSession) -> EniStatus;
    pub fn eni_session_stop(session: *mut EniSession) -> EniStatus;
    pub fn eni_session_pause(session: *mut EniSession) -> EniStatus;
    pub fn eni_session_resume(session: *mut EniSession) -> EniStatus;
    pub fn eni_session_get_state(session: *const EniSession) -> EniSessionState;
    pub fn eni_session_elapsed_ms(session: *const EniSession) -> u64;
    pub fn eni_session_set_subject(
        session: *mut EniSession,
        subject_id: *const c_char,
    ) -> EniStatus;
    pub fn eni_session_destroy(session: *mut EniSession);

    // dsp
    pub fn eni_dsp_fft_init(ctx: *mut EniDspFftCtx, size: i32) -> EniStatus;
    pub fn eni_dsp_extract_features(
        ctx: *mut EniDspFftCtx,
        signal: *const f32,
        n: i32,
        sample_rate: f32,
        features: *mut EniDspFeatures,
    ) -> EniStatus;
}

// ---------------------------------------------------------------------------
// Error type
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
pub struct EniError {
    pub status: EniStatus,
    pub message: String,
}

impl fmt::Display for EniError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "eNI error {:?}: {}", self.status, self.message)
    }
}

impl std::error::Error for EniError {}

impl From<EniStatus> for EniError {
    fn from(status: EniStatus) -> Self {
        let msg = unsafe {
            let ptr = eni_status_str(status);
            if ptr.is_null() {
                "unknown error".to_string()
            } else {
                CStr::from_ptr(ptr).to_string_lossy().into_owned()
            }
        };
        EniError {
            status,
            message: msg,
        }
    }
}

pub type Result<T> = std::result::Result<T, EniError>;

fn check(status: EniStatus) -> Result<()> {
    if status == EniStatus::Ok {
        Ok(())
    } else {
        Err(EniError::from(status))
    }
}

// ---------------------------------------------------------------------------
// Safe wrapper: Config
// ---------------------------------------------------------------------------

pub struct Config {
    inner: Box<EniConfig>,
}

impl Config {
    pub fn new() -> Result<Self> {
        unsafe {
            let mut cfg = Box::new(std::mem::zeroed::<EniConfig>());
            check(eni_config_init(cfg.as_mut()))?;
            Ok(Config { inner: cfg })
        }
    }

    pub fn load_file(&mut self, path: &str) -> Result<()> {
        let c_path = CString::new(path).map_err(|_| EniError {
            status: EniStatus::ErrInvalid,
            message: "invalid path string".into(),
        })?;
        unsafe { check(eni_config_load_file(self.inner.as_mut(), c_path.as_ptr())) }
    }

    pub fn load_defaults(&mut self, variant: EniVariant) -> Result<()> {
        unsafe { check(eni_config_load_defaults(self.inner.as_mut(), variant)) }
    }

    pub fn as_ptr(&self) -> *const EniConfig {
        &*self.inner
    }

    pub fn as_mut_ptr(&mut self) -> *mut EniConfig {
        &mut *self.inner
    }
}

impl Default for Config {
    fn default() -> Self {
        Self::new().expect("eni_config_init failed")
    }
}

// ---------------------------------------------------------------------------
// Safe wrapper: Session
// ---------------------------------------------------------------------------

pub struct Session {
    inner: Box<EniSession>,
}

impl Session {
    pub fn new() -> Result<Self> {
        unsafe {
            let mut session = Box::new(std::mem::zeroed::<EniSession>());
            check(eni_session_init(session.as_mut()))?;
            Ok(Session { inner: session })
        }
    }

    pub fn start(&mut self) -> Result<()> {
        unsafe { check(eni_session_start(self.inner.as_mut())) }
    }

    pub fn stop(&mut self) -> Result<()> {
        unsafe { check(eni_session_stop(self.inner.as_mut())) }
    }

    pub fn pause(&mut self) -> Result<()> {
        unsafe { check(eni_session_pause(self.inner.as_mut())) }
    }

    pub fn resume(&mut self) -> Result<()> {
        unsafe { check(eni_session_resume(self.inner.as_mut())) }
    }

    pub fn state(&self) -> EniSessionState {
        unsafe { eni_session_get_state(&*self.inner) }
    }

    pub fn elapsed_ms(&self) -> u64 {
        unsafe { eni_session_elapsed_ms(&*self.inner) }
    }

    pub fn set_subject(&mut self, subject_id: &str) -> Result<()> {
        let c_id = CString::new(subject_id).map_err(|_| EniError {
            status: EniStatus::ErrInvalid,
            message: "invalid subject_id string".into(),
        })?;
        unsafe { check(eni_session_set_subject(self.inner.as_mut(), c_id.as_ptr())) }
    }
}

impl Drop for Session {
    fn drop(&mut self) {
        unsafe {
            eni_session_destroy(self.inner.as_mut());
        }
    }
}

// ---------------------------------------------------------------------------
// Safe wrapper: SignalProcessor
// ---------------------------------------------------------------------------

pub struct SignalProcessor {
    ctx: Box<EniDspFftCtx>,
}

impl SignalProcessor {
    pub fn new(fft_size: i32) -> Result<Self> {
        unsafe {
            let mut ctx = Box::new(std::mem::zeroed::<EniDspFftCtx>());
            check(eni_dsp_fft_init(ctx.as_mut(), fft_size))?;
            Ok(SignalProcessor { ctx })
        }
    }

    pub fn extract_features(
        &mut self,
        signal: &[f32],
        sample_rate: f32,
    ) -> Result<EniDspFeatures> {
        unsafe {
            let mut features = std::mem::zeroed::<EniDspFeatures>();
            check(eni_dsp_extract_features(
                self.ctx.as_mut(),
                signal.as_ptr(),
                signal.len() as i32,
                sample_rate,
                &mut features,
            ))?;
            Ok(features)
        }
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn status_display() {
        let err = EniError::from(EniStatus::ErrInvalid);
        assert!(err.message.len() > 0);
    }
}
