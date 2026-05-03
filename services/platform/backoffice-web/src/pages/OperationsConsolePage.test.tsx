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
    expect(screen.getByText("联盟管理")).toBeTruthy();
    expect(screen.getByText("内容管理")).toBeTruthy();
    expect(screen.getByText("治理审核")).toBeTruthy();
  });

  it("adds partner in affiliate tab", async () => {
    render(<OperationsConsolePage api={new FakeGatewayRepository()} />);
    const input = screen.getByLabelText("新增伙伴");
    fireEvent.change(input, { target: { value: "测试伙伴" } });
    fireEvent.click(screen.getByText("新增"));
    await waitFor(() => {
      expect(screen.getByText(/测试伙伴/)).toBeTruthy();
    });
  });

  it("toggles content status in content tab", async () => {
    render(<OperationsConsolePage api={new FakeGatewayRepository()} />);
    fireEvent.click(screen.getByText("内容管理"));
    fireEvent.click(screen.getByText("下架"));
    await waitFor(() => {
      expect(screen.getByText(/topic_1001/)).toBeTruthy();
    });
  });

  it("approves pending review in governance tab", async () => {
    render(<OperationsConsolePage api={new FakeGatewayRepository()} />);
    fireEvent.click(screen.getByText("治理审核"));
    fireEvent.click(screen.getByText("通过"));
    await waitFor(() => {
      expect(screen.getByText(/review.status.update/)).toBeTruthy();
    });
  });
});
