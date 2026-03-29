-- user-domain PostgreSQL schema (v1). Apply on service startup or via migration job.

CREATE TABLE IF NOT EXISTS user_account (
    user_id TEXT PRIMARY KEY,
    is_guest BOOLEAN NOT NULL DEFAULT FALSE,
    display_name TEXT,
    avatar_url TEXT,
    locale TEXT,
    bio TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS user_session (
    session_id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,
    access_token TEXT NOT NULL UNIQUE,
    refresh_token TEXT NOT NULL UNIQUE,
    is_guest BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS guest_device (
    device_key TEXT PRIMARY KEY,
    session_id TEXT NOT NULL REFERENCES user_session (session_id) ON DELETE CASCADE,
    user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS user_preferences_blob (
    user_id TEXT PRIMARY KEY REFERENCES user_account (user_id) ON DELETE CASCADE,
    prefs_hex TEXT
);

CREATE TABLE IF NOT EXISTS user_favorite (
    favorite_id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,
    guide_card_id TEXT NOT NULL,
    UNIQUE (user_id, guide_card_id)
);

CREATE TABLE IF NOT EXISTS user_history (
    owner_key TEXT NOT NULL,
    guide_card_id TEXT NOT NULL,
    PRIMARY KEY (owner_key, guide_card_id)
);

CREATE TABLE IF NOT EXISTS user_consent_blob (
    user_id TEXT PRIMARY KEY REFERENCES user_account (user_id) ON DELETE CASCADE,
    consent_hex TEXT
);

CREATE TABLE IF NOT EXISTS user_signal (
    user_key TEXT PRIMARY KEY,
    signal_ref TEXT NOT NULL,
    bundle_version TEXT NOT NULL DEFAULT 'v1'
);

CREATE TABLE IF NOT EXISTS user_feedback (
    feedback_id TEXT PRIMARY KEY,
    actor_user_id TEXT,
    actor_session_id TEXT,
    target_type INT,
    target_id TEXT,
    client_request_id TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
