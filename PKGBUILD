# Mantenedor: Alvaro <alvaro@localhost>
pkgname=zapshell
pkgver=0.1.0
pkgrel=1
pkgdesc='Cliente nativo do WhatsApp Web em Qt6 WebEngine, com tray, notificacoes e backup'
arch=('x86_64')
license=('MIT')
depends=('qt6-base' 'qt6-webengine>=6.10' 'qt6-webchannel' 'hicolor-icon-theme')
makedepends=('cmake' 'git')
source=()
sha256sums=()

# Os fontes vem do HEAD do repositorio local onde makepkg e executado. Assim o
# pacote contem exatamente o commit que esta sendo empacotado, sem build/ nem
# spike/ (que ficam untracked no diretorio de trabalho).
build() {
    git -C "$startdir" archive --format=tar --prefix="$pkgname-$pkgver/" HEAD | tar -C "$srcdir" -x

    cmake -S "$srcdir/$pkgname-$pkgver" -B "$srcdir/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$srcdir/build" --parallel "$(nproc)"
}

check() {
    ctest --test-dir "$srcdir/build" --output-on-failure
}

package() {
    install -Dm755 "$srcdir/build/zapshell" "$pkgdir/usr/bin/zapshell"
    install -Dm644 "$srcdir/$pkgname-$pkgver/assets/zapshell.desktop" \
        "$pkgdir/usr/share/applications/zapshell.desktop"
    install -Dm644 "$srcdir/$pkgname-$pkgver/assets/zapshell.svg" \
        "$pkgdir/usr/share/icons/hicolor/scalable/apps/zapshell.svg"
    install -Dm644 "$srcdir/$pkgname-$pkgver/assets/zapshell-tray.svg" \
        "$pkgdir/usr/share/icons/hicolor/scalable/apps/zapshell-tray.svg"

    for unit in zapshell.service zapshell-backup.service zapshell-backup.timer; do
        install -Dm644 "$srcdir/$pkgname-$pkgver/assets/$unit" \
            "$pkgdir/usr/lib/systemd/user/$unit"
    done

    install -Dm644 "$srcdir/$pkgname-$pkgver/README.md" "$pkgdir/usr/share/doc/$pkgname/README.md"
    install -Dm644 "$srcdir/$pkgname-$pkgver/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}

post_install() {
    cat <<'EOF'

zapshell instalado.

Autostart (opcional):
  systemctl --user daemon-reload
  systemctl --user enable --now zapshell.service

Atalho no Hyprland (Ryoku) — adicione a linha abaixo em
~/.config/hypr/modules/binds.lua:

  hl.bind(K(mod .. " + W"), function() hl.exec_cmd("zapshell --toggle") end)

Veja /usr/share/doc/zapshell/README.md para limitacoes, backup e restore.
EOF
}
