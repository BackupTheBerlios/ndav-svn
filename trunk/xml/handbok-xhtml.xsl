<?xml version="1.0" ?>

<!--
	Usage:  xsltproc handbok{-xhtml.xsl,.xml}
-->

<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:fo="http://www.w3.org/1999/XSL/Format" >

	<!-- Stylesheet location for Debian GNU/Linux. -->
	<xsl:import href="file:///usr/share/xml/docbook/stylesheet/nwalsh/xhtml/chunk.xsl" />

	<xsl:param name="chunk.quietly">1</xsl:param>
	<xsl:param name="chunker.output.encoding">UTF-8</xsl:param>
	<xsl:param name="use.id.as.filename">1</xsl:param>
	<xsl:param name="base.dir">xhtml/</xsl:param>
	<xsl:param name="html.stylesheet">ndav.css</xsl:param>

  <!-- Simplify text insertion when cross-referencing `refsect1' -->
	<xsl:param name="local.l10n.xml" select="document('')"/>
	<l:i18n xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0">
		<l:l10n language="en">
			<l:context name="xref">
				<l:template name="refsect1" text="the section &#8220;%t&#8221;"/>
			</l:context>
		</l:l10n>
	</l:i18n>

</xsl:stylesheet>
