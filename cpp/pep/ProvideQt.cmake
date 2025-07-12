find_package(Qt6 REQUIRED COMPONENTS Core Network Gui PrintSupport Widgets LinguistTools Svg)

# Check for use of deprecated APIs before the given version
add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x060600)  # New syntax (Qt >=6.5)
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x060600) # Old syntax (Qt <6.5)
