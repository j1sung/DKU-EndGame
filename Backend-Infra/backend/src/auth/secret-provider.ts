export interface ClientSecretProvider {
  getClientSecret(): Promise<string>;
}
