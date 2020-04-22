# Maintainer: Alexander Mohr keyboard_backlight@mohr.io

pkgname=tp-kb-backlight-git
pkgver=1.0
pkgrel=1
pkgdesc='Automated keyboard backlight'
arch=('x86_64')
url='https://github.com/alexmohr/keyboard-backlight'
license=('MIT')
depends=('libinput')
makedepends=('git' 'cmake' 'gcc')
git='keyboard-backlight'
appname='keyboard_backlight'

source=("git+https://github.com/alexmohr/keyboard-backlight")
sha512sums=('SKIP')


build() {
    cd "${git}"
    git checkout dev
    mkdir build -p
    cd build
    cmake ..
    make
}

package() {
    install -Dm 755 "${git}/build/${appname}" "${pkgdir}/usr/bin/${appname}"
    install -Dm 644 "${git}/keyboard_backlight.service" "${pkgdir}/etc/systemd/system/keyboard_backlight.service"

    echo "Please enable the systemd service via 'sudo systemctl enable --now keyboard_backlight.service"
    echo "The service file can be used for configuration."
    echo "See 'keyboard_backlight -h' for options".
}
