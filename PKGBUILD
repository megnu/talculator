# Maintainer: local

pkgname=talculator
pkgver=2.1.4
pkgrel=1
pkgdesc='GTK3 scientific calculator with tab support (talculator fork)'
arch=('x86_64')
url='https://github.com/megnu/talculator'
license=('GPL')
depends=('gtk3' 'libqalculate')
makedepends=('intltool' 'autoconf' 'automake' 'libtool')
source=()
sha256sums=()

build() {
  cd "${startdir}"

  # Required because upstream still has duplicate global definitions
  # (for example prefs in main.c and config_file.c).
  CFLAGS+=' -fcommon'

  autoreconf -fi
  ./configure --prefix=/usr
  make
}

package() {
  cd "${startdir}"
  make DESTDIR="${pkgdir}" install

  # Match current Arch packaging layout.
  if [[ -d "${pkgdir}/usr/share/appdata" ]]; then
    mv "${pkgdir}/usr/share/appdata" "${pkgdir}/usr/share/metainfo"
  fi
}
