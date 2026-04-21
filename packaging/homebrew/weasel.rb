# Formula for the weasel developer package:
#   weaselc + weasel_lsp_server binaries, libweasel.a, libweasel.dylib/.so, and headers.
# Lives in a Homebrew tap: https://github.com/weasel-lang/weasel-homebrew.git
# Install: brew install weasel-lang/weasel
#
# The sha256 fields and version are updated automatically by the release workflow.
# DO NOT edit manually — run `scripts/update-homebrew-formula.sh <version>` instead.

class Weasel < Formula
  desc "C++ HTML generation library with CCX transpiler and LSP server (developer package)"
  homepage "https://github.com/weasel-lang/weasel-lsp"
  version "0.1.0"
  license "MIT"

  on_arm do
    on_macos do
      url "https://github.com/weasel-lang/weasel-lsp/releases/download/v#{version}/weasel-v#{version}-macos-arm64.tar.gz"
      sha256 "PLACEHOLDER_MACOS_ARM64"
    end
  end

  on_linux do
    if Hardware::CPU.arm?
      url "https://github.com/weasel-lang/weasel-lsp/releases/download/v#{version}/weasel-v#{version}-linux-arm64.tar.gz"
      sha256 "PLACEHOLDER_LINUX_ARM64"
    else
      url "https://github.com/weasel-lang/weasel-lsp/releases/download/v#{version}/weasel-v#{version}-linux-x86_64.tar.gz"
      sha256 "PLACEHOLDER_LINUX_X86_64"
    end
  end

  def install
    bin.install "bin/weaselc"
    bin.install "bin/weasel_lsp_server"
    lib.install "lib/libweasel.a"
    on_macos do
      lib.install "lib/libweasel.dylib"
    end
    on_linux do
      lib.install "lib/libweasel.so"
    end
    include.install "include/weasel"
  end

  test do
    system "#{bin}/weaselc", "--version"
    assert_predicate lib/"libweasel.a", :exist?
    assert_predicate include/"weasel/weasel.hpp", :exist?
  end
end
