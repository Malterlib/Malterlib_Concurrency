// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_DistributedApp.h"

#include <Mib/Cryptography/UUID>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Process/Platform>
#include <Mib/Concurrency/LocalHostIdentity>

namespace NMib::NConcurrency
{
	using namespace NEncoding;
	using namespace NFile;
	using namespace NStr;
	using namespace NContainer;
	using namespace NStorage;
	using namespace NNetwork;
	using namespace NCryptography;

	namespace
	{
		struct CDefaultSettingsDefault
		{
			CDistributedAppActor_SettingsProperties m_Properties{CDistributedAppActor_SettingsProperties::CDefaultInit{}};
		};
	}

	constinit NStorage::TCAggregate<CDefaultSettingsDefault> g_DefaultSettingsDefault = {DAggregateInit};

	CDistributedAppActor_SettingsProperties &CDistributedAppActor_Settings::fs_GetGlobalDefaultSettings()
	{
		return g_DefaultSettingsDefault->m_Properties;
	}

	CDistributedAppActor_SettingsProperties::CDistributedAppActor_SettingsProperties(CDefaultInit _Dummy)
	{
	}

	CDistributedAppActor_SettingsProperties::CDistributedAppActor_SettingsProperties()
	{
		*this = g_DefaultSettingsDefault->m_Properties;
	}

	namespace
	{
		CUniversallyUniqueIdentifier g_HostnameRootUUID("8ED61926-AC8A-4793-92C5-DE05547999E7", EUniversallyUniqueIdentifierFormat_Bare);
		constexpr ch8 const *gc_pLocalSocketHostnamePrefix = "UNIX(666):";
	}

	auto CDistributedAppActor_InterfaceSettings::fs_ParseEnvironmentOptions(CStr const &_Options) -> EOption
	{
		EOption OptionFlags = EOption_None;

		CStr Options = _Options;
		while (!Options.f_IsEmpty())
		{
			CStr Option = fg_GetStrSep(Options, ";");
			if (Option == "DelegateTrust")
				OptionFlags |= EOption_DelegateTrustToAppInterface;
		}

		return OptionFlags;
	}

	CDistributedAppActor_Settings::CDistributedAppActor_Settings()
		: CDistributedAppActor_SettingsProperties(fg_DistributedAppThreadLocal().m_DefaultSettings)
	{
	}

	CDistributedAppActor_Settings::CDistributedAppActor_Settings(NStr::CStr const &_AppName)
		: CDistributedAppActor_SettingsProperties(fg_DistributedAppThreadLocal().m_DefaultSettings)
	{
		if (fg_DistributedAppThreadLocal().m_bRemoteCommandLine)
			m_AppName = "{}.Remote"_f << _AppName;
		else
			m_AppName = _AppName;

		if (m_AuditCategory.f_IsEmpty())
			m_AuditCategory = _AppName;
#ifndef DPlatformFamily_Windows
		// Validate the longest variant, including .wsa, so the fallback name always fits.
		DMibRequire
			(
				fp_GetLocalSocketPath
				(
					fg_Format("/tmp/{}", g_HostnameRootUUID.f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum))
					, ELocalSocketFlag::mc_EnclaveSpecific | ELocalSocketFlag::mc_AuthenticatedUnix
					, m_Enclave
				)
				.f_GetLen()
				<= aint(NSys::NNetwork::fg_GetMaxUnixSocketNameLength())
			)
		;
#else
		if (!CFile::fs_GetDrive(m_RootDirectory).f_IsEmpty())
			m_RootDirectory = m_RootDirectory.f_Replace(CFile::fs_GetDrive(m_RootDirectory), CFile::fs_GetDrive(m_RootDirectory).f_LowerCase());
#endif
	}

	NStr::CStr CDistributedAppActor_Settings::fp_GetLocalSocketPath(CStr const &_Prefix, ELocalSocketFlag _Flags, CStr const &_Enclave) const
	{
		// App names containing .wsa can collide across transports; compare without case for case-insensitive filesystems.
		DMibRequire(m_Enclave.f_IsEmpty() || CActorDistributionManager::fs_IsValidEnclave(m_Enclave));
		DMibRequire(!m_AppName.f_LowerCase().f_EndsWith(".wsa") && m_AppName.f_LowerCase().f_Find(".wsa.") < 0);

		CStr TransportSuffix = fg_IsSet(_Flags, ELocalSocketFlag::mc_AuthenticatedUnix) ? gc_Str<".wsa">.m_Str : CStr();
		if (!m_Enclave.f_IsEmpty() && fg_IsSet(_Flags, ELocalSocketFlag::mc_EnclaveSpecific))
			return fg_Format("{}/{}{}.{}.socket", _Prefix, m_AppName, TransportSuffix, _Enclave);
		else
			return fg_Format("{}/{}{}.socket", _Prefix, m_AppName, TransportSuffix);
	}

	// The same relative spelling of distinct roots must not share a socket directory
	NStr::CStr CDistributedAppActor_Settings::fp_GetExpandedRootDirectoryHash() const
	{
		return fg_GetHashedUuidString(CFile::fs_GetExpandedPath(m_RootDirectory), g_HostnameRootUUID, EUniversallyUniqueIdentifierFormat_AlphaNum);
	}

	// The directory for this root under MalterlibDistributedAppLocalSocketPrefix, or empty if the variable is unset or empty. The app saves the base address, not
	// the enclave socket path, so a client that uses the saved address needs a hostname map to reach the enclave socket. The client builds that map
	// from its own environment, so the app and its command lines run with the same prefix
	NStr::CStr CDistributedAppActor_Settings::f_GetLocalSocketPrefixDirectory() const
	{
		CStr SocketPrefix = fg_GetSys()->f_GetEnvironmentVariable("MalterlibDistributedAppLocalSocketPrefix");
		if (SocketPrefix.f_IsEmpty())
			return {};

		return CFile::fs_GetExpandedPath(SocketPrefix / fp_GetExpandedRootDirectoryHash());
	}

	// Whether _FileName has the base socket name for _Flags, without enclave, in the root directory, by expanded path or by identity, or in a directory
	// named by a hash of the root, as the directories under a prefix and the fallback directories are. Blocking, since the identity check reads the filesystem
	bool CDistributedAppActor_Settings::f_IsLocalSocketFileNameOfRoot(NStr::CStr const &_FileName, ELocalSocketFlag _Flags) const
	{
		if (CFile::fs_GetFile(_FileName) != CFile::fs_GetFile(fp_GetLocalSocketPath({}, _Flags & ~ELocalSocketFlag::mc_EnclaveSpecific, {})))
			return false;

		CStr RootDirectory = m_RootDirectory;
#ifdef DPlatformFamily_Windows
		RootDirectory = CFile::fs_GetExpandedPath(RootDirectory);
#endif
		// The earlier run can have spelled the same root differently, relative or absolute, or reached it through a link, as /tmp is on macOS
		CStr Directory = CFile::fs_GetPath(_FileName);
		if (CFile::fs_GetExpandedPath(Directory) == CFile::fs_GetExpandedPath(RootDirectory))
			return true;

		if
		(
			CFile::fs_FileExists(Directory, EFileAttrib_Directory)
			&& CFile::fs_FileExists(RootDirectory, EFileAttrib_Directory)
			&& CFile::fs_GetUniqueIdentifier(Directory) == CFile::fs_GetUniqueIdentifier(RootDirectory)
		)
		{
			return true;
		}

		// A fallback directory is named by a hash of the root, so only those of the current spelling and of the expanded root are known as this
		// root's: the hash of another spelling an earlier run used can't be told apart from a directory of another root, so the app's state records the
		// listen it made outside the root directory
		CStr DirectoryName = CFile::fs_GetFile(Directory);
		return DirectoryName == fg_GetHashedUuidString(RootDirectory, g_HostnameRootUUID, EUniversallyUniqueIdentifierFormat_AlphaNum) || DirectoryName == fp_GetExpandedRootDirectoryHash();
	}

	// Whether _Hostname is one f_GetLocalSocketHostname gives for _Flags, without enclave, at a location f_IsLocalSocketFileNameOfRoot accepts.
	// A unix host with other permissions is a listen configured explicitly, not one the command line setup creates
	bool CDistributedAppActor_Settings::f_IsLocalSocketHostnameOfRoot(NStr::CStr const &_Hostname, ELocalSocketFlag _Flags) const
	{
		if (!fs_IsLocalSocketHostname(_Hostname))
			return false;

		return f_IsLocalSocketFileNameOfRoot(_Hostname.f_Extract(fg_StrLen(gc_pLocalSocketHostnamePrefix)), _Flags);
	}

	// Whether _Hostname has the UNIX(666): prefix that f_GetLocalSocketHostname gives. Doesn't access the filesystem
	bool CDistributedAppActor_Settings::fs_IsLocalSocketHostname(NStr::CStr const &_Hostname)
	{
		return _Hostname.f_StartsWith(gc_pLocalSocketHostnamePrefix);
	}

	// The URL of the listen the command line setup creates for _Flags at _Hostname
	NWeb::NHTTP::CURL CDistributedAppActor_Settings::fs_GetLocalSocketURL(NStr::CStr const &_Hostname, ELocalSocketFlag _Flags)
	{
		// The socket path comes from the root directory or the prefix, and can contain characters the URL reserves
		CStr EncodedHostname = NWeb::NHTTP::CURL::fs_PercentEncode(_Hostname, "[]%@");
		return NWeb::NHTTP::CURL{fg_Format("{}://[{}]/", fg_IsSet(_Flags, ELocalSocketFlag::mc_AuthenticatedUnix) ? "wsa" : "wss", EncodedHostname)};
	}

	// Whether _URL has a host f_IsLocalSocketHostnameOfRoot accepts and equals the URL fs_GetLocalSocketURL gives for that host and _Flags. A listen
	// configured explicitly with the same URL can't be told apart
	bool CDistributedAppActor_Settings::f_IsLocalSocketListenOfRoot(NWeb::NHTTP::CURL const &_URL, ELocalSocketFlag _Flags) const
	{
		if (!f_IsLocalSocketHostnameOfRoot(_URL.f_GetHost(), _Flags))
			return false;

		return _URL == fs_GetLocalSocketURL(_URL.f_GetHost(), _Flags);
	}

	// Blocking; a server calls it before it listens. Throws for a link, a non-directory and, on POSIX, a directory owned by another user, who could
	// replace its sockets. Only the directory itself and the ancestors it creates are checked: untrusted users must not be able to replace it or
	// any of its existing ancestors
	void CDistributedAppActor_Settings::fs_PrepareLocalSocketDirectory(NStr::CStr const &_Directory)
	{
		if (_Directory.f_IsEmpty())
			return;

		auto fExists = [](CStr const &_Path)
			{
				return CFile::fs_FileExists(_Path, EFileAttrib_Directory | EFileAttrib_File | EFileAttrib_Link);
			}
		;

		// Ordered from the outermost, so that on POSIX each one is restricted before the ones below it are checked
		TCVector<CStr> CreatedDirectories;
		for (CStr Directory = _Directory; !Directory.f_IsEmpty() && !fExists(Directory); Directory = CFile::fs_GetPath(Directory))
			CreatedDirectories.f_InsertFirst(Directory);

		// An owner-only Unix mode keeps other users from adding entries before the checks below. Inherited ACL entries still apply, and Windows ignores the mode
		if (!CreatedDirectories.f_IsEmpty())
		{
			constexpr EFileAttrib c_OwnerOnly = NFile::EFileAttrib_UnixAttributesValid | NFile::EFileAttrib_UserExecute | NFile::EFileAttrib_UserRead | NFile::EFileAttrib_UserWrite;
			CFile::fs_CreateDirectory(_Directory, c_OwnerOnly, c_OwnerOnly);
		}
		else
			CreatedDirectories.f_Insert(_Directory);

#ifndef DPlatformFamily_Windows
		CStr User = NSys::fg_UserManagement_GetProcessEffectiveUserName();
#endif
		for (auto &Directory : CreatedDirectories)
		{
			EFileAttrib DirectoryAttributes = CFile::fs_GetAttributesOnLink(Directory);
			if ((DirectoryAttributes & EFileAttrib_Link) || !(DirectoryAttributes & EFileAttrib_Directory))
				DMibError("Local socket directory '{}' is not a directory"_f << Directory);

#ifndef DPlatformFamily_Windows
			CStr Owner = CFile::fs_GetOwnerOnLink(Directory);
			if (Owner != User)
				DMibError("Local socket directory '{}' is owned by '{}' instead of '{}'"_f << Directory << Owner << User);

			// An existing directory can be writable by the group. Other users only get search permission from the mode, so that they can reach the known
			// socket path without listing or changing the directories
			CFile::fs_SetAttributes
				(
					Directory
					, NFile::EFileAttrib_UnixAttributesValid
					| NFile::EFileAttrib_UserExecute
					| NFile::EFileAttrib_UserRead
					| NFile::EFileAttrib_UserWrite
					| NFile::EFileAttrib_GroupExecute
					| NFile::EFileAttrib_EveryoneExecute
				)
			;
#endif
		}
	}

	// Resolves the path only, without creating or preparing its directory
	NStr::CStr CDistributedAppActor_Settings::f_GetLocalSocketFileName(ELocalSocketFlag _Flags, NStr::CStr const &_Enclave) const
	{
		// Fallback directory selection belongs to this instance's m_Enclave; another instance needs its own settings.
		umint MaxLength = NSys::NNetwork::fg_GetMaxUnixSocketNameLength();
		CStr RootDirectory = m_RootDirectory;
#ifdef DPlatformFamily_Windows
		RootDirectory = CFile::fs_GetExpandedPath(RootDirectory);
#endif
		CStr ConfigHash = fg_GetHashedUuidString(RootDirectory, g_HostnameRootUUID, EUniversallyUniqueIdentifierFormat_AlphaNum);

		CStr PrefixDirectory = f_GetLocalSocketPrefixDirectory();

		// A prefix means the root's filesystem may not host sockets, and the environment's temporary directory can be on it
		if (!PrefixDirectory.f_IsEmpty())
		{
			if (fp_GetLocalSocketPath(PrefixDirectory, _Flags | ELocalSocketFlag::mc_EnclaveSpecific, m_Enclave).f_GetLen() <= aint(MaxLength))
				return fp_GetLocalSocketPath(PrefixDirectory, _Flags, _Enclave);

			ConfigHash = fp_GetExpandedRootDirectoryHash();
		}
		else
		{
			if (fp_GetLocalSocketPath(RootDirectory, _Flags | ELocalSocketFlag::mc_EnclaveSpecific, m_Enclave).f_GetLen() <= aint(MaxLength))
				return fp_GetLocalSocketPath(RootDirectory, _Flags, _Enclave);

			CStr TempDir = CFile::fs_GetRawTemporaryDirectory();
#ifdef DPlatformFamily_Windows
			TempDir = CFile::fs_GetExpandedPath(TempDir);
#endif
			CStr Prefix = TempDir / ConfigHash;
			if (fp_GetLocalSocketPath(Prefix, _Flags | ELocalSocketFlag::mc_EnclaveSpecific, m_Enclave).f_GetLen() <= aint(MaxLength))
				return fp_GetLocalSocketPath(Prefix, _Flags, _Enclave);
		}

		CStr Prefix = fg_Format("/tmp/{}", ConfigHash);
#ifdef DPlatformFamily_Windows
		Prefix = CFile::fs_GetExpandedPath(Prefix);
#endif
		DMibCheck(fp_GetLocalSocketPath(Prefix, _Flags | ELocalSocketFlag::mc_EnclaveSpecific, m_Enclave).f_GetLen() <= aint(MaxLength));

		return fp_GetLocalSocketPath(Prefix, _Flags, _Enclave);
	}

	NStr::CStr CDistributedAppActor_Settings::f_GetLocalSocketWildcard(ELocalSocketFlag _Flags) const
	{
		// Each transport needs a wildcard because .wsa can change its fallback directory.
		return f_GetLocalSocketFileName(_Flags, "*");
	}

	// A wildcard for the temporary paths, from fg_GetStagedUnixSocketPath, that listeners bind before they rename them to the sockets f_GetLocalSocketWildcard
	// matches. Needs a nonempty m_Enclave and mc_EnclaveSpecific in _Flags. Only the file name becomes a pattern, since the directory can contain '*' on POSIX
	NStr::CStr CDistributedAppActor_Settings::f_GetStagedLocalSocketWildcard(ELocalSocketFlag _Flags) const
	{
		CStr Wildcard = f_GetLocalSocketWildcard(_Flags);
		CStr FileName = CFile::fs_GetFile(Wildcard);

		return CFile::fs_GetPath(Wildcard) / (FileName.f_Extract(0, FileName.f_FindChar('*')) + "*~*");
	}

	NStr::CStr CDistributedAppActor_Settings::f_GetLocalSocketHostname(ELocalSocketFlag _Flags) const
	{
		return fg_Format("{}{}", gc_pLocalSocketHostnamePrefix, f_GetLocalSocketFileName(_Flags, m_Enclave));
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_RootDirectory(CStr const &_RootDirectory) &&
	{
		m_RootDirectory = _RootDirectory;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_SeparateDistributionManager(bool _bSeparateDistributionManager) &&
	{
		m_bSeparateDistributionManager = _bSeparateDistributionManager;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_KeySetting(NCryptography::CPublicKeySetting const &_KeySetting) &&
	{
		m_KeySetting = _KeySetting;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_MinimumCryptoStrength(NCryptography::ECryptoStrength _Strength) &&
	{
		m_MinimumCryptoStrength = _Strength;

		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_FriendlyName(CStr const &_FriendlyName) &&
	{
		m_FriendlyName = _FriendlyName;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_Enclave(CStr const &_Enclave) &&
	{
		m_Enclave = _Enclave;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_AuditCategory(NStr::CStr const &_Category) &&
	{
		m_AuditCategory = _Category;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_RunAsUser(NStr::CStr const &_User) &&
	{
		m_RunAsUser = _User;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_RunAsGroup(NStr::CStr const &_Group) &&
	{
		m_RunAsGroup = _Group;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_UpdateType(EDistributedAppUpdateType _UpdateType) &&
	{
		m_UpdateType = _UpdateType;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_InterfaceSettings(CDistributedAppActor_InterfaceSettings const &_InterfaceSettings) &&
	{
		m_InterfaceSettings = _InterfaceSettings;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_SupportUserAuthentication(bool _bSupportUserAuthentication) &&
	{
		m_bSupportUserAuthentication = _bSupportUserAuthentication;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_CanUseLocalListenAsPrimary(bool _bCanUseLocalListenAsPrimary) &&
	{
		m_bCanUseLocalListenAsPrimary = _bCanUseLocalListenAsPrimary;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_CommandLineBeforeAppStart(bool _bCommandLineBeforeAppStart) &&
	{
		m_bCommandLineBeforeAppStart = _bCommandLineBeforeAppStart;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_WaitForRemotes(bool _bWaitForRemotes) &&
	{
		m_bWaitForRemotes = _bWaitForRemotes;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_TimeoutForUnixSockets(bool _bTimeoutForUnixSockets) &&
	{
		m_bTimeoutForUnixSockets = _bTimeoutForUnixSockets;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_TLSForLocalSockets(bool _bTLSForLocalSockets) &&
	{
		m_bTLSForLocalSockets = _bTLSForLocalSockets;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_InProcessCommandLineOnly(bool _bInProcessCommandLineOnly) &&
	{
		m_bInProcessCommandLineOnly = _bInProcessCommandLineOnly;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_NotRunningMessage(NStr::CStr const &_Message) &&
	{
		m_NotRunningMessage = _Message;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_ReconnectDelay(fp64 _ReconnectDelay) &&
	{
		m_ReconnectDelay = _ReconnectDelay;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_HostTimeoutOnShutdown(fp64 _HostTimeoutOnShutdown) &&
	{
		m_HostTimeoutOnShutdown = _HostTimeoutOnShutdown;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_KillHostsTimeoutOnShutdown(fp64 _KillHostsTimeoutOnShutdown) &&
	{
		m_KillHostsTimeoutOnShutdown = _KillHostsTimeoutOnShutdown;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_MaxShutdownTime(fp64 _MaxShutdownTime) &&
	{
		m_MaxShutdownTime = _MaxShutdownTime;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_DefaultCommandLineFunctionalies(EDefaultCommandLineFunctionality _DefaultCommandLineFunctionality) &&
	{
		m_DefaultCommandLineFunctionality = _DefaultCommandLineFunctionality;
		return fg_Move(*this);
	}

	// May synchronously query the local identity when no friendly name is configured.
	NStr::CStr CDistributedAppActor_Settings::f_GetCompositeFriendlyName() const
	{
		if (!m_FriendlyName.f_IsEmpty())
			return fg_Format("{}/{}", m_FriendlyName, m_AppName);

		return f_GetCompositeFriendlyName(fg_GetLocalHostIdentityNow());
	}

	// Uses the supplied identity for an unconfigured friendly name; performs no lookup.
	NStr::CStr CDistributedAppActor_Settings::f_GetCompositeFriendlyName(CLocalHostIdentity const &_Identity) const
	{
		CStr FriendlyName = m_FriendlyName;
		if (FriendlyName.f_IsEmpty())
			FriendlyName = _Identity.f_UserAtComputer();

		return fg_Format("{}/{}", FriendlyName, m_AppName);
	}
}
