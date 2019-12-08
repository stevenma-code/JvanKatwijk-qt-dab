#
/*
 *    Copyright (C) 2013 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef	__XMLFILE_HANDLER__
#define	__XMLFILE_HANDLER__

#include	<QThread>
#include	<QString>
#include	<QFrame>
#include	<atomic>
#include	"dab-constants.h"
#include	"virtual-input.h"
#include	"ringbuffer.h"
#include	"filereader-widget.h"

class	QLabel;
class	QSettings;
class	xmlDescriptor;
class	xmlfileReader;
/*
 */
class	xmlfileHandler: public virtualInput,
	                public filereaderWidget {
Q_OBJECT
public:
				xmlfileHandler	(QString);
                		~xmlfileHandler	();
	int32_t			getSamples	(std::complex<float> *,
	                                                         int32_t);
	uint8_t			myIdentity	();
	int32_t			Samples		();
	bool			restartReader	(int32_t);
	void			stopReader	(void);
private:
	QString			fileName;
	RingBuffer<std::complex<float>>	*_I_Buffer;
	FILE			*theFile;
	uint32_t		filePointer;
	xmlDescriptor		*theDescriptor;
	xmlfileReader		*theReader;
	std::atomic<bool>	running;
public slots:
	void			setProgress	(int, int);
};

#endif

