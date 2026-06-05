import { cleanup, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { afterEach, describe, expect, it } from "vitest";
import { FakeGatewayRepository } from "../gateway/fakeRepository";
import { OperationsConsolePage } from "./OperationsConsolePage";

afterEach(() => {
  cleanup();
});

describe("OperationsConsolePage", () => {
  it("renders three domain tabs", async () => {
    render(<OperationsConsolePage api={new FakeGatewayRepository()} />);
    expect(screen.getAllByText("联盟管理").length).toBeGreaterThan(0);
    expect(screen.getAllByText("内容管理").length).toBeGreaterThan(0);
    expect(screen.getAllByText("治理审核").length).toBeGreaterThan(0);
  });

  it("adds partner in affiliate tab", async () => {
    render(<OperationsConsolePage api={new FakeGatewayRepository()} />);
    fireEvent.click(screen.getAllByText("联盟管理")[0]);
    const input = screen.getByLabelText("伙伴名称");
    fireEvent.change(input, { target: { value: "tmall" } });
    fireEvent.click(screen.getByText("新增伙伴"));
    await waitFor(() => {
      expect(screen.getByText(/tmall/)).toBeTruthy();
    });
  });

  it("toggles content status in content tab", async () => {
    render(<OperationsConsolePage api={new FakeGatewayRepository()} />);
    await waitFor(() => {
      expect(screen.getByText(/topic_1001/)).toBeTruthy();
    });
    fireEvent.click(screen.getByText("下架"));
    await waitFor(() => {
      expect(screen.getByText(/topic_1001/)).toBeTruthy();
    });
  });

  it("approves pending review in governance tab", async () => {
    render(<OperationsConsolePage api={new FakeGatewayRepository()} />);
    fireEvent.click(screen.getAllByText("治理审核")[0]);
    fireEvent.click(screen.getByText("通过"));
    await waitFor(() => {
      expect(screen.getByText(/review.status.update/)).toBeTruthy();
    });
  });
});
