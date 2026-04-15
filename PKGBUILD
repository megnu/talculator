# Maintainer: local

pkgname=talculator
pkgver=2.1.4
pkgrel=1
pkgdesc='GTK3 scientific calculator with tab support (talculator fork)'
arch=('x86_64')
url='https://github.com/megnu/talculator'
license=('GPL')
depends=('gtk3' 'libqalculate')
makedepends=('git' 'intltool' 'autoconf' 'automake' 'libtool')
source=()
sha256sums=()

_builddir="${pkgname}-${pkgver}"

prepare() {
  rm -rf "${srcdir}/${_builddir}"
  mkdir -p "${srcdir}/${_builddir}"
  git -C "${startdir}" archive --format=tar HEAD | tar -xf - -C "${srcdir}/${_builddir}"
}

build() {
  cd "${srcdir}/${_builddir}"

  # Required because upstream still has duplicate global definitions
  # (for example prefs in main.c and config_file.c).
  CFLAGS+=' -fcommon'

  autoreconf -fi
  ./configure --prefix=/usr
  make
}

package() {
  cd "${srcdir}/${_builddir}"
  make DESTDIR="${pkgdir}" install

  # Match current Arch packaging layout.
  if [[ -d "${pkgdir}/usr/share/appdata" ]]; then
    mv "${pkgdir}/usr/share/appdata" "${pkgdir}/usr/share/metainfo"
  fi
}
