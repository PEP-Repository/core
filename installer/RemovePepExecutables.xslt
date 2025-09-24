<?xml version="1.0" encoding="utf-8"?>
<!-- Ignore PEP executables that are already handled by binaries.wsx, such that they will not also be picked up by Heat -->
<!-- Based on: https://stackoverflow.com/questions/44765707/how-to-exclude-files-in-wix-toolset -->
<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:wix="http://schemas.microsoft.com/wix/2006/wi"
  xmlns="http://schemas.microsoft.com/wix/2006/wi"

  version="1.0"
  exclude-result-prefixes="xsl wix"
>
  <xsl:output method="xml" indent="yes" omit-xml-declaration="yes"/>

  <xsl:strip-space elements="*"/>

  <!--
  Find all <Component> elements with <File> elements with Source="" attributes containing PEP executable keys and tag them with the "Remove" key.

  <Component Id="cmpXXX" Guid="*">
      <File Id="filYYY" KeyPath="yes" Source="$(var.ArtifactsDir)\pepAssessor.exe" />
  </Component>

  Because WiX's Heat.exe only supports XSLT 1.0 and not XSLT 2.0 we cannot use `ends-with(haystack, needle)` (see https://github.com/wixtoolset/issues/issues/5609 )
  -->
  <xsl:key
    name="Remove"
    match="wix:Component[
      contains(wix:File/@Source, 'pepAssessor.exe') or
      contains(wix:File/@Source, 'pepElevate.exe') or
      contains(wix:File/@Source, 'pepLogon.exe') or
      contains(wix:File/@Source, 'pepcli.exe')
    ]"
    use="@Id"
  />

  <!-- By default, copy all elements and nodes into the output... -->
  <xsl:template match="@*|node()">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
  </xsl:template>

  <!-- ...but if the element has the "Remove" key then don't render anything (i.e. removing it from the output) -->
  <xsl:template match="*[ self::wix:Component or self::wix:ComponentRef ][ key('Remove', @Id) ]"/>

</xsl:stylesheet>
