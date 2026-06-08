-- user-server PostgreSQL schema. Apply on service startup or via migration job.

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
    app_id TEXT NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS guest_device (
    device_key TEXT PRIMARY KEY,
    session_id TEXT NOT NULL REFERENCES user_session (session_id) ON DELETE CASCADE,
    user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS user_preferences_blob (
    app_id TEXT NOT NULL DEFAULT '',
    user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,
    prefs_hex TEXT,
    PRIMARY KEY (app_id, user_id)
);

CREATE TABLE IF NOT EXISTS user_favorite (
    favorite_id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,
    content_id TEXT NOT NULL,
    content_type INT NOT NULL DEFAULT 0,
    app_id TEXT NOT NULL DEFAULT '',
    favorited_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (app_id, user_id, content_id)
);

CREATE TABLE IF NOT EXISTS user_history (
    owner_key TEXT NOT NULL,
    content_id TEXT NOT NULL,
    content_type INT NOT NULL DEFAULT 0,
    app_id TEXT NOT NULL DEFAULT '',
    last_seen_at TIMESTAMPTZ,
    first_seen_at TIMESTAMPTZ,
    impression_count INT NOT NULL DEFAULT 1,
    source_surface INT NOT NULL DEFAULT 0,
    PRIMARY KEY (owner_key, content_id)
);

CREATE TABLE IF NOT EXISTS user_consent_blob (
    app_id TEXT NOT NULL DEFAULT '',
    user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,
    consent_hex TEXT,
    PRIMARY KEY (app_id, user_id)
);

CREATE TABLE IF NOT EXISTS user_signal (
    user_key TEXT PRIMARY KEY,
    signal_ref TEXT NOT NULL,
    bundle_version TEXT NOT NULL DEFAULT 'v1',
    app_id TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS user_feedback (
    feedback_id TEXT PRIMARY KEY,
    actor_user_id TEXT,
    actor_session_id TEXT,
    target_type INT,
    target_id TEXT,
    client_request_id TEXT,
    app_id TEXT NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
