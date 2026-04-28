// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
package com.eos.eni;

/**
 * Java/Android bindings for the eNI session API.
 *
 * <p>Manages a BCI session lifecycle (init, start, stop) via JNI calls
 * into the native libeni library.
 */
public class EniSession implements AutoCloseable {

    static {
        System.loadLibrary("eni_jni");
    }

    /** Session states mirroring eni_session_state_t. */
    public enum State {
        IDLE,
        CALIBRATING,
        RUNNING,
        PAUSED,
        STOPPED,
        ERROR;

        static State fromOrdinal(int ordinal) {
            State[] values = values();
            if (ordinal >= 0 && ordinal < values.length) {
                return values[ordinal];
            }
            return ERROR;
        }
    }

    /** Status codes mirroring eni_status_t. */
    public enum Status {
        OK,
        ERR_NOMEM,
        ERR_INVALID,
        ERR_NOT_FOUND,
        ERR_IO,
        ERR_TIMEOUT,
        ERR_PERMISSION,
        ERR_RUNTIME,
        ERR_CONNECT,
        ERR_PROTOCOL,
        ERR_CONFIG,
        ERR_UNSUPPORTED,
        ERR_POLICY_DENIED,
        ERR_PROVIDER,
        ERR_OVERFLOW;

        static Status fromOrdinal(int ordinal) {
            Status[] values = values();
            if (ordinal >= 0 && ordinal < values.length) {
                return values[ordinal];
            }
            return ERR_RUNTIME;
        }
    }

    // Native peer handle (pointer stored as long).
    private long nativeHandle;
    private boolean closed;

    /**
     * Create and initialise a new eNI session.
     *
     * @throws EniException if native initialisation fails
     */
    public EniSession() throws EniException {
        nativeHandle = nativeInit();
        if (nativeHandle == 0) {
            throw new EniException("Failed to initialise eNI session", Status.ERR_RUNTIME);
        }
        closed = false;
    }

    /**
     * Start the session.
     *
     * @throws EniException on failure
     */
    public void start() throws EniException {
        ensureOpen();
        int rc = nativeStart(nativeHandle);
        checkStatus(rc, "start");
    }

    /**
     * Stop the session.
     *
     * @throws EniException on failure
     */
    public void stop() throws EniException {
        ensureOpen();
        int rc = nativeStop(nativeHandle);
        checkStatus(rc, "stop");
    }

    /**
     * Pause the session.
     *
     * @throws EniException on failure
     */
    public void pause() throws EniException {
        ensureOpen();
        int rc = nativePause(nativeHandle);
        checkStatus(rc, "pause");
    }

    /**
     * Resume a paused session.
     *
     * @throws EniException on failure
     */
    public void resume() throws EniException {
        ensureOpen();
        int rc = nativeResume(nativeHandle);
        checkStatus(rc, "resume");
    }

    /**
     * Get the current session state.
     *
     * @return current {@link State}
     */
    public State getState() {
        if (closed) {
            return State.STOPPED;
        }
        return State.fromOrdinal(nativeGetState(nativeHandle));
    }

    /**
     * Get elapsed session time in milliseconds.
     *
     * @return elapsed milliseconds
     */
    public long getElapsedMs() {
        if (closed) {
            return 0;
        }
        return nativeGetElapsedMs(nativeHandle);
    }

    /**
     * Set the subject identifier for this session.
     *
     * @param subjectId subject identifier string
     * @throws EniException on failure
     */
    public void setSubject(String subjectId) throws EniException {
        ensureOpen();
        int rc = nativeSetSubject(nativeHandle, subjectId);
        checkStatus(rc, "setSubject");
    }

    /** Release native resources. */
    @Override
    public void close() {
        if (!closed && nativeHandle != 0) {
            nativeDestroy(nativeHandle);
            nativeHandle = 0;
            closed = true;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        close();
        super.finalize();
    }

    // -- internal helpers --

    private void ensureOpen() throws EniException {
        if (closed || nativeHandle == 0) {
            throw new EniException("Session is closed", Status.ERR_INVALID);
        }
    }

    private static void checkStatus(int rc, String operation) throws EniException {
        if (rc != 0) {
            throw new EniException(
                    "eNI " + operation + " failed with status " + rc,
                    Status.fromOrdinal(rc));
        }
    }

    // -- native methods --

    private static native long nativeInit();
    private static native int  nativeStart(long handle);
    private static native int  nativeStop(long handle);
    private static native int  nativePause(long handle);
    private static native int  nativeResume(long handle);
    private static native int  nativeGetState(long handle);
    private static native long nativeGetElapsedMs(long handle);
    private static native int  nativeSetSubject(long handle, String subjectId);
    private static native void nativeDestroy(long handle);

    // -- exception class --

    public static class EniException extends Exception {
        private final Status status;

        public EniException(String message, Status status) {
            super(message);
            this.status = status;
        }

        public Status getStatus() {
            return status;
        }
    }
}
