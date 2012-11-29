<?xml version="1.0" ?>

<!--
	Usage:  xsltproc handbok.x{s,m}l
-->

<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:fo="http://www.w3.org/1999/XSL/Format" >

	<!-- Stylesheet location for Debian GNU/Linux. -->
	<xsl:import href="file:///usr/share/xml/docbook/stylesheet/nwalsh/manpages/docbook.xsl" />

	<xsl:param name="man.output.base.dir">man/</xsl:param>
	<xsl:param name="man.output.in.separate.dir">1</xsl:param>
	<xsl:param name="man.output.subdirs.enabled">0</xsl:param>
	<xsl:param name="man.output.lang.in.name.enabled">1</xsl:param>
	<xsl:param name="man.authors.section.enabled">0</xsl:param>

  <!-- Simplify text insertion when cross-referencing `refsect1' -->
	<xsl:param name="local.l10n.xml" select="document('')"/>
	<l:i18n xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0">
		<l:l10n language="en">
			<l:context name="xref">
				<l:template name="refsect1" text="the section ``%t''"/>
			</l:context>
		</l:l10n>
	</l:i18n>

</xsl:stylesheet>
