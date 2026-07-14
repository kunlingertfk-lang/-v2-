HALCON_ROOT = /opt/halcon

HALCON_VERSION_HEADER = $$HALCON_ROOT/include/HVersNum.h
!exists($$HALCON_VERSION_HEADER) {
    error("HALCON 20.11 header not found: $$HALCON_VERSION_HEADER")
}
!system(grep -Eq "HLIB_MAJOR_NUM[[:space:]]+20" $$shell_quote($$HALCON_VERSION_HEADER)) {
    error("HALCON major version must be 20: $$HALCON_VERSION_HEADER")
}
!system(grep -Eq "HLIB_MINOR_NUM[[:space:]]+11" $$shell_quote($$HALCON_VERSION_HEADER)) {
    error("HALCON minor version must be 11: $$HALCON_VERSION_HEADER")
}

INCLUDEPATH += $$HALCON_ROOT/include
message("Using HALCON 20.11 root: $$HALCON_ROOT")
