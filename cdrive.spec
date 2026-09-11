Name:           cdrive
Version:        1.0.3
Release:        1%{?dist}
Summary:        Professional command-line interface for Google Drive

License:        MIT
URL:            https://github.com/batuhantrkgl/CDrive
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(json-c)

%description
cdrive is a professional, lightweight command-line interface for Google Drive.
Features include OAuth2 authentication, interactive folder browsing, resumable
downloads, multi-file uploads, search, and structured JSON output.

%prep
%autosetup -n CDrive-%{version}

%build
%set_build_flags
%make_build

%install
%make_install PREFIX=%{_prefix} BINDIR=%{_bindir}

%check
%{buildroot}%{_bindir}/%{name} help >/dev/null

%files
%license LICENSE
%doc README.md
%{_bindir}/%{name}

%changelog
* Sat Sep 12 2026 Batuhan Türkoğlu <batuhanturkoglu37@gmail.com> - 1.0.3-1
- Initial release of cdrive 1.0.3 for Fedora COPR
