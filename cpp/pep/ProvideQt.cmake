find_package(Qt6 REQUIRED COMPONENTS Core Network Gui PrintSupport Widgets LinguistTools Svg)

# Qt by default warns on deprecated APIs (via QT_WARN_DEPRECATED_UP_TO),
# but we choose to disable some deprecated APIs altogether before the given version
add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x060800)  # New syntax (Qt >=6.5)
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x060800) # Old syntax (Qt <6.5)
