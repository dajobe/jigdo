%define version			0.7.2
%define release			0
%define buildfordefault 	Red Hat Linux

# My rather hacky distribution selector: not very sophisticated but it SEEMS to WORK!
# use 'rpmbuild --define '_vendor redhat' to build for Red Hat Linux
# use 'rpmbuild --define '_vendor mandrake' to build for Mandrake (untested, sorry!)
# use 'rpmbuild --define '_vendor whatever' to build for Whatever (but please implement it yourself!)
# If no command line _vendor macro is entered, the _vendor macro of the buildsystem is used
# (see: 'rpmbuild --showrc | grep _vendor'). If the buildsystem doesn't have a _vendor macro
# we'll use Red Hat Linux ... and just see how far we'll get!
%if %{!?_vendor:1}%{?_vendor:0}
	%{expand: %{warn:No _vendor available: default settings (%{buildfordefault}) will be used!}}
	%{expand: %%define _vendor redhat}
%endif
# Gotcha! In bash true = 0 but in rpm-specfile-macros true = 1!
%if %([[ %{_vendor} == "redhat" ]] && echo 1 || echo 0)
	%{expand: %%define buildforredhat 1}
	%{expand: %{echo:Red Hat Linux settings will be used!}}
%else
	%{expand: %%define buildforredhat 0}
%endif
%if %([[ %{_vendor} == "mandrake" ]] && echo 1 || echo 0)
	%{expand: %%define buildformandrake 1}
	%{expand: %{warn:Mandrake settings in this specfile are untested}}
%else
	%{expand: %%define buildformandrake 0}
%endif
%if %([[ %{_vendor} == "suse" ]] && echo 1 || echo 0)
	%{expand: %%define buildforsuse 0}
	%{expand: %%define buildforredhat 1}
	%{expand: %{error:SuSE settings in this specfile are not implemented}}
	%{expand: %{echo:Red Hat Linux settings will be used!}}
%else
	%{expand: %%define buildforsuse 0}
%endif
%if %([[ %{_vendor} != "redhat" && %{_vendor} != "mandrake" && %{_vendor} != "suse" ]] && echo 1 || echo 0)
	%{expand: %{warn:%{_vendor} unknown: Default settings (%{buildfordefault}) will be used}}
	%{expand: %%define buildforredhat 1}
%endif


%define name		jigdo
%define title		Jigdo
%define summary		Jigsaw Download
%define icon		jigdo.png


%if %{buildforredhat}
%{expand: %%define jigdoprefix	%{_prefix}}
%{expand: %%define _menudir	%{_prefix}/share/applications}
%{expand: %%define menufile	%{name}.desktop}
%{expand: %%define _bindir	%{jigdoprefix}/bin}
%{expand: %%define _datadir	%{jigdoprefix}/share}
%{expand: %%define _liconsdir	%{_prefix}/share/icons/hicolor/48x48/apps}
%{expand: %%define _iconsdir	%{_prefix}/share/icons/hicolor/32x32/apps}
%{expand: %%define _miconsdir	%{_prefix}/share/icons/hicolor/16x16/apps}
%{expand: %%define _mandir	%{jigdoprefix}/man}
%{expand: %%define group	Applications/Internet}
%{expand: %%define configure	./configure --prefix=%{jigdoprefix}}
%{expand: %%define make		make}
%{expand: %%define makeinstall_std	make DESTDIR=%{buildroot} install}
BuildRequires:	compat-db, w3c-libwww-devel, gawk, zlib-devel, gtk2-devel, ImageMagick
Requires:	compat-db, w3c-libwww, wget
%endif

%if %{buildformandrake}
%{expand: %%define _menudir	%{_libdir}/menu}
%{expand: %%define menufile	%{name}}
%{expand: %%define _iconsdir	%{_datadir}/icons}
%{expand: %%define _miconsdir	%{_datadir}/icons/mini}
%{expand: %%define _liconsdir	%{_datadir}/icons/large}
%{expand: %%define longtitle	%{summary}}
%{expand: %%define group	Networking/File transfer}
%{expand: %%define section	%{group}}
# Update Menu
%{expand: %%define _update_menus_bin %{_bindir}/update-menus}
%{expand: %%define update_menus if [ -x %{_update_menus_bin} ]; then %{_update_menus_bin} || true ; fi}
# Clean Menu
%{expand: %%define clean_menus if [ "$1" = "0" -a -x %{_update_menus_bin} ]; then %{_update_menus_bin} || true ; fi}
BuildRequires:	libdb3.3-devel, w3c-libwww-devel, mawk, libopenssl0-devel
Requires:	libdb3.3, w3c-libwww, libopenssl0, common-licenses
%endif

%if %{buildforsuse}
%{expand: %%define jigdoprefix	fake}
%{expand: %%define _menudir	fake}
%{expand: %%define menufile	fake}
%{expand: %%define _iconsdir	fake}
%{expand: %%define _miconsdir	fake}
%{expand: %%define _liconsdir	fake}
%{expand: %%define _mandir	fake}
%{expand: %%define group	fake}
%{expand: %%define configure	fake}
%{expand: %%define make		fake}
%{expand: %%define makeinstall_std	fake}
BuildRequires:	fake
Requires:	fake
%endif


Summary:	%{summary}
Name:		%{name}
Version:	%{version}
Release:	%{release}
Group:		%{group}
URL:		http://atterer.net/jigdo/
Source:		http://atterer.net/jigdo/%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root
License:	GPL


%description
Jigsaw Download, or short jigdo, is an intelligent tool that can be used on the
pieces of any chopped-up big file to create a special "template" file which
makes reassembly of the file very easy for users who only have the pieces.

What makes jigdo special is that there are no restrictions on what
offsets/sizes the individual pieces have in the original big image. This makes
the program very well suited for distributing CD/DVD images (or large zip/tar
archives) because you can put the files of the CD on an FTP server - when jigdo
is presented the files along with the template you generated, it is able to
recreate the CD image.


%prep
%setup -q


%build
%configure
%make


%install
rm -rf %{buildroot}

%makeinstall_std	
mkdir -p %{buildroot}{%{_liconsdir},%{_iconsdir},%{_miconsdir}}
convert gfx/jigdo-icon.png -geometry 48 %{buildroot}%{_liconsdir}/%{icon}
convert gfx/jigdo-icon.png -geometry 32 %{buildroot}%{_iconsdir}/%{icon}
convert gfx/jigdo-icon.png -geometry 16 %{buildroot}%{_miconsdir}/%{icon}


# Menu stuff
mkdir -p %{buildroot}%{_menudir}
%if %{buildforredhat}
cat > %{buildroot}%{_menudir}/%{menufile} <<EOF
[Desktop Entry]
Encoding=UTF-8
Type=Application
Exec=%{_bindir}/%{name}
Icon=%{_iconsdir}/%{icon}
Terminal=0
Name=%{name}
Comment=%{summary}
Categories=Application;Network;X-Red-Hat-Extra;
EOF
%endif
%if %{buildformandrake}
cat > %buildroot%{_menudir}/%{menufile} << EOF
?package(%{name}): \
    command="%{_bindir}/%{name}" \
    title="%{title}" \
    longtitle="%{longtitle}" \
    section="%{section}" \
    icon="%{icon}" \
    needs="x11"
EOF

%post
# This will only execute, if %{_update_menus_bin} (see above)
# is executable
%{update_menus}

%postun
# This will only execute, if %{_update_menus_bin} (see above)
# is executable
%{clean_menus}

%endif


%{find_lang} %{name}


%clean
rm -rf %{buildroot}


%files -f %{name}.lang
%defattr(-,root,root)
%doc README doc/TechDetails.txt doc/*.html
%{_mandir}/man1/%{name}*
%{_bindir}
%{_datadir}/%{name}
%{_menudir}/%{menufile}
%{_liconsdir}/%{icon}
%{_iconsdir}/%{icon}
%{_miconsdir}/%{icon}


%changelog
* Sun Jun 1 2003 Paul Bolle <jigdo-rpm@atterer.net> 0.7.0-5
- Some optimizations (too lazy to specifiy)
- Small typo in %%description
- Added dependency: ImageMagick (Red Hat Linux)
- added --prefix =/usr to configure (Red Hat Linux)
- preliminary work for SuSE (mostly fake!)

* Fri May 16 2003 Paul Bolle <jigdo-rpm@atterer.net> 0.7.0-4
- Minimization of use of buildforredhat and buildformandrake 
- Minor optimizations

* Tue May 13 2003 Paul Bolle <jigdo-rpm@atterer.net> 0.7.0-3
- Use jigdo-icon.png instead of 3 png.bz2 of mandrake.src.rpm
- Deleted double listing of pixmaps in %%files

* Sun May 11 2003 Paul Bolle <jigdo-rpm@atterer.net> 0.7.0-2
- Richard Atterer reminded me that I had suggested to use %%{_vendor}
- Deleted VERSION from %%doc and added all html's
- Added build-dependencies: zlib-devel, gtk2-devel (Red Hat Linux)
- Added dependencie: wget (Red Hat Linux)

* Sat May 10 2003 Paul Bolle <jigdo-rpm@atterer.net> 0.7.0-1
- First Red Hat Linux 9 enabled rewrite

* Sat Dec 28 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.8-3mdk
- Rebuild for new glibc

* Thu Sep 05 2002 Lenny Cartier <lenny@mandrakesoft.com> 0.6.8-2mdk
- rebuild

* Sat Jul 20 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.8-1mdk
- 0.6.8

* Thu Jun  6 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.7-1mdk
- 0.6.7
- Remove gcc 3.1 patch - merged upstream
- Fix download source

* Fri May 31 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.6-1mdk
- 0.6.6
- Add patch to make it compile with gcc 3.1.1

* Fri Apr 26 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.5-1mdk
- After constant reminders by the author, here's the (not that much 
  anymore) brand new version of the academy award winning jigdo - 
  Version 0.6.5! ;)

* Tue Mar  5 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.4-1mdk
- 0.6.4

* Sun Feb  8 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.2-3mdk
- Jigdo compiles with gcc 2.96 now

* Sat Jan 24 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.2-2mdk
- Make the SPEC be generic, so that it can be built on non-Mandrake
  machines

* Sat Jan 24 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.2-1mdk
- 0.6.2
- Remove patch1 - merged upstream

* Tue Jan 22 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.1-1mdk
- First Mandrake release
