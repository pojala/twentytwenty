
20:20 (a.k.a. TwentyTwenty)
Read Me

---------------------------

Author: Pauli Olavi Ojala
Contact: pauli (at) lacquer (dot) fi
Last updated: 2009.03.20

---------------------------

20:20 is a lightweight multimedia creation and distribution application that
works on both handheld and desktop computers. Its design is language-
independent and does not require literacy. The same program can be used to
both view and create documents.

Each 20:20 document ("20book") consists of 20 pages that can contain three
types of content: photographs, resolution-independent vector graphics and up
to 20 seconds of voice audio. By drawing, speaking and taking photos, users
with no previous computer skills can produce sophisticated 20books in their
own language.

20:20's file format uses efficient compression: a document containing color
vector graphics and several minutes of spoken audio can fit in less than 100
kB, which makes it practical to transmit documents over basic email or MMS.
This is crucial for enabling the distribution of 20books over mobile networks
in developing countries.

Visit the project's web site for more information:
   http://anioni.com/2020


The application is written in plain C and is intended to be highly portable.
Currently it has been ported to Mac OS X and Maemo (a Linux variant used on
Nokia's internet tablets).

20:20 uses the Ogg file format, and the amazing audio compression is achieved
using the Speex codec. Enormous thanks to the authors of these tremendously
useful open source projects!

If you'd like to port 20:20 to another operating system, have a look
in the "maemo" directory (which contains the Maemo-specific files) to get
an idea of what needs to be implemented or modified for a new port.
Please feel free to contact the author (email above) if you have any
questions or need some help!


---------------------------

Copyright (c) 2008-2009, Pauli Olavi Ojala

20:20 is free software that is licensed under the GNU General Public License.
Please see TwentyTwenty_LICENSE.txt.

The application also contains copies of certain libraries that use BSD-style
source licenses. The copyright notices and licenses for these libraries
follow:


--- "Ogg" library ---
Copyright (c) 2002, Xiph.org Foundation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of the Xiph.org Foundation nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


--- "OggZ" library ---
Copyright (C) 2003 Commonwealth Scientific and Industrial Research
Organisation (CSIRO) Australia

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of CSIRO Australia nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


--- "Speex" library ---
Copyright 2002-2008 	Xiph.org Foundation
Copyright 2002-2008 	Jean-Marc Valin
Copyright 2005-2007	Analog Devices Inc.
Copyright 2005-2008	Commonwealth Scientific and Industrial Research 
                        Organisation (CSIRO)
Copyright 1993, 2002, 2006 David Rowe
Copyright 2003 		EpicGames
Copyright 1992-1994	Jutta Degener, Carsten Bormann

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of the Xiph.org Foundation nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


---------------------------
(end)