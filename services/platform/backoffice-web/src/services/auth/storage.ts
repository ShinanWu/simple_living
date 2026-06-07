const TOKEN_KEY = "backoffice_token";
const ROLE_KEY = "backoffice_role";

export function getBackofficeToken(): string | null {
  return localStorage.getItem(TOKEN_KEY);
}

export function getBackofficeRole(): string {
  return localStorage.getItem(ROLE_KEY) ?? "backoffice_admin";
}

export function setBackofficeSession(token: string, role: string): void {
  localStorage.setItem(TOKEN_KEY, token);
  localStorage.setItem(ROLE_KEY, role);
}

export function clearBackofficeSession(): void {
  localStorage.removeItem(TOKEN_KEY);
  localStorage.removeItem(ROLE_KEY);
}

export function isBackofficeLoggedIn(): boolean {
  return Boolean(getBackofficeToken());
}
