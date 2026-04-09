import SwiftUI
import SimpleLivingCore

/// 登录面板：手机号 OTP 与微信（联调可填授权码；生产接入微信 SDK）。
struct LoginView: View {
    @Environment(\.dismiss) private var dismiss
    @StateObject private var viewModel: LoginFlowViewModel

    @State private var phoneE164 = ""
    @State private var otpCode = ""
    @State private var verificationId = ""
    @State private var weChatOpenId = ""
    @State private var weChatAuthCode = ""

    private let onSuccess: () -> Void

    init(api: GatewayAPI, onSuccess: @escaping () -> Void) {
        _viewModel = StateObject(wrappedValue: LoginFlowViewModel(api: api))
        self.onSuccess = onSuccess
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Text("微信一键登录为默认主路径；以下为联调字段，上架前替换为 SDK 授权。")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }

                Section("微信登录") {
                    TextField("provider_subject（如 openid）", text: $weChatOpenId)
                        .textInputAutocapitalization(.never)
                    TextField("authorization_code", text: $weChatAuthCode)
                        .textInputAutocapitalization(.never)
                    Button {
                        Task {
                            let ok = await viewModel.loginWithWeChat(
                                providerSubject: weChatOpenId,
                                authorizationCode: weChatAuthCode
                            )
                            if ok {
                                onSuccess()
                                dismiss()
                            }
                        }
                    } label: {
                        if viewModel.busy {
                            ProgressView()
                        } else {
                            Text("微信登录")
                        }
                    }
                    .disabled(viewModel.busy || weChatOpenId.isEmpty || weChatAuthCode.isEmpty)
                }

                Section("手机号登录") {
                    TextField("+86138…", text: $phoneE164)
                        .keyboardType(.phonePad)
                    TextField("验证码", text: $otpCode)
                        .keyboardType(.numberPad)
                    TextField("verification_id", text: $verificationId)
                        .textInputAutocapitalization(.never)
                    Button {
                        Task {
                            let ok = await viewModel.loginWithPhone(
                                phoneE164: phoneE164,
                                otpCode: otpCode,
                                verificationId: verificationId
                            )
                            if ok {
                                onSuccess()
                                dismiss()
                            }
                        }
                    } label: {
                        if viewModel.busy {
                            ProgressView()
                        } else {
                            Text("手机号登录")
                        }
                    }
                    .disabled(viewModel.busy || phoneE164.isEmpty || otpCode.isEmpty || verificationId.isEmpty)
                }

                if let err = viewModel.errorMessage {
                    Section {
                        Text(err).foregroundStyle(.red)
                    }
                }
            }
            .navigationTitle("登录")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("关闭") { dismiss() }
                }
            }
        }
    }
}
