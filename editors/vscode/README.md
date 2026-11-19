# SPL language support for VS Code

This local extension provides syntax highlighting and basic editor behavior
for `.spl` files.

## Install locally

In VS Code:

1. Open the Command Palette.
2. Run **Extensions: Install from VSIX...** after packaging this directory, or
   use **Developer: Install Extension from Location...** if available.
3. Select `editors/vscode`.

For development, open `editors/vscode` as an extension development folder and
press `F5` to launch an Extension Development Host.

The extension currently provides TextMate coloring, comments, bracket matching,
and indentation. Parser-backed diagnostics, completion, and navigation should
be added later through an SPL language server.
