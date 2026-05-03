#include <iostream>

#include "pg_user_store.h"

namespace {

int g_failures = 0;

void Expect(bool cond, const char* message) {
    if (!cond) {
        std::cerr << "[FAIL] " << message << std::endl;
        ++g_failures;
    }
}

}  // namespace

int main() {
    simple_living::user_domain::PgUserStore store;

    // Basic lifecycle safety: default instance should be safe to ping/close.
    Expect(!store.Ping(), "Ping should be false before connect");
    store.Close();
    store.Close();
    Expect(!store.Ping(), "Ping should remain false after Close without connect");

    // Invalid conninfo should fail cleanly and keep store in disconnected state.
    const bool ok = store.ConnectAndInit(
        "host=127.0.0.1 port=1 dbname=missing user=missing password=missing connect_timeout=1");
    Expect(!ok, "ConnectAndInit should fail for invalid local PostgreSQL endpoint");
    Expect(!store.Ping(), "Ping should be false after failed connect");

    if (g_failures == 0) {
        std::cout << "pg_user_store_unit_test passed" << std::endl;
        return 0;
    }
    return 1;
}
