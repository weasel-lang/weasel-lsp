# Formula for the weaselc transpiler and weasel_lsp_server.
# Lives in a Homebrew tap: https://github.com/weasel-lang/homebrew-weasel
# Install: brew install weasel-lang/weasel/weaselc
#
# The sha256 fields and version are updated automatically by the release workflow.
# DO NOT edit manually — run `scripts/update-homebrew-formula.sh <version>` instead.

class Weaselc < Formula
  desc "Transpiler and LSP server for CCX (C++ Components eXtended) — JSX-like templates in C++"
  homepage "https://github.com/weasel-lang/language-services-weasel"
  version "0.1.0"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/weasel-lang/language-services-weasel/releases/download/v#{version}/weasel-v#{version}-macos-arm64.tar.gz"
      sha256 "PLACEHOLDER_MACOS_ARM64"
    else
      url "https://github.com/weasel-lang/language-services-weasel/releases/download/v#{version}/weasel-v#{version}-macos-x86_64.tar.gz"
      sha256 "PLACEHOLDER_MACOS_X86_64"
    end
  end

  on_linux do
    if Hardware::CPU.arm?
      url "https://github.com/weasel-lang/language-services-weasel/releases/download/v#{version}/weasel-v#{version}-linux-arm64.tar.gz"
      sha256 "PLACEHOLDER_LINUX_ARM64"
    else
      url "https://github.com/weasel-lang/language-services-weasel/releases/download/v#{version}/weasel-v#{version}-linux-x86_64.tar.gz"
      sha256 "PLACEHOLDER_LINUX_X86_64"
    end
  end

  def install
    bin.install "weaselc"
    bin.install "weasel_lsp_server"
  end

  test do
    system "#{bin}/weaselc", "--version"
    assert_predicate bin/"weasel_lsp_server", :executable?
  end
end
