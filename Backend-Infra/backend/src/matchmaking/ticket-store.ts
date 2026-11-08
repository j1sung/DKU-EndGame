export interface TicketMutation {
  playerId: string;
  idempotencyKey: string;
  requestHash: string;
  requestId: string;
}

export interface TicketView {
  ticketId: string;
  status: "WAITING" | "ASSIGNED";
  expiresAt?: string;
  matchId?: string;
}

export interface TicketStore {
  create(input: TicketMutation): Promise<TicketView & { requestId: string }>;
  current(playerId: string): Promise<TicketView | null>;
  cancel(input: TicketMutation & { ticketId: string }): Promise<{ ticketId: string; status: "CANCELLED"; requestId: string }>;
}
