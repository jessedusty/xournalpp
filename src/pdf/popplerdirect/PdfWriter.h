/*
 * Xournal++
 *
 * Part of the PDF export
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "PdfXRef.h"
#include <StringUtils.h>

#include <gtk/gtk.h>

#include <boost/filesystem/path.hpp>
using boost::filesystem::path;

#define FORMAT(f, ...) \
	StringUtils::format(f, __VA_ARGS__)

class PdfWriter
{
public:
	PdfWriter(PdfXRef* xref);
	virtual ~PdfWriter();

public:
	void close();
	bool openFile(path filename);

public:
	bool write(string data);
	bool writeTxt(string data);
	bool write(int data);

	void startStream();
	void endStream();

	bool writeInfo(string title);
	bool writeObj();


	string getLastError();

	int getObjectId();
	int getNextObjectId();
	int getDataCount();

public:
	static void setCompressPdfOutput(bool compress);

private:
	XOJ_TYPE_ATTRIB;

	static bool compressPdfOutput;

	bool inStream;
	string stream;

	GFileOutputStream* out;

	string lastError;

	PdfXRef* xref;

	int objectId;

};
