using Ludork.Plugin.Abstractions;
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.BlueprintAssistant;

public sealed class PluginSecretStore : IPluginSecretStore
{
    private const uint CredentialTypeGeneric = 1;
    private const uint CredentialPersistLocalMachine = 2;
    private const int ErrorNotFound = 1168;
    private const int MacOSItemNotFound = -25300;
    private readonly string pluginId;

    public PluginSecretStore(string pluginId)
    {
        if (string.IsNullOrWhiteSpace(pluginId))
            throw new ArgumentException("Plugin id is required.", nameof(pluginId));
        this.pluginId = pluginId.Trim();
    }

    public async Task<bool> ContainsAsync(
        string name,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (OperatingSystem.IsWindows())
            return readWindows(name) is not null;
        if (OperatingSystem.IsMacOS())
            return await readMacOSAsync(name, cancellationToken) is not null;
        return Environment.GetEnvironmentVariable(getEnvironmentName(name)) is not null;
    }

    public async Task<string?> ReadAsync(
        string name,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (OperatingSystem.IsWindows())
            return readWindows(name);
        if (OperatingSystem.IsMacOS())
            return await readMacOSAsync(name, cancellationToken);
        return Environment.GetEnvironmentVariable(getEnvironmentName(name));
    }

    public async Task WriteAsync(
        string name,
        string value,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (OperatingSystem.IsWindows())
        {
            writeWindows(name, value);
            return;
        }
        if (OperatingSystem.IsMacOS())
        {
            await writeMacOSAsync(name, value, cancellationToken);
            return;
        }
        throw new PlatformNotSupportedException(
            $"Set {getEnvironmentName(name)} to provide this secret on the current platform.");
    }

    public async Task DeleteAsync(
        string name,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (OperatingSystem.IsWindows())
        {
            deleteWindows(name);
            return;
        }
        if (OperatingSystem.IsMacOS())
        {
            await deleteMacOSAsync(name, cancellationToken);
            return;
        }
        throw new PlatformNotSupportedException(
            $"Remove {getEnvironmentName(name)} from the process environment to delete this secret.");
    }

    private string getTarget(string name)
    {
        if (string.IsNullOrWhiteSpace(name))
            throw new ArgumentException("Secret name is required.", nameof(name));
        return "Ludork/" + pluginId + "/" + name.Trim();
    }

    private string getEnvironmentName(string name)
    {
        StringBuilder result = new("LUDORK_PLUGIN_SECRET_");
        foreach (char character in pluginId + "_" + name)
        {
            result.Append(char.IsLetterOrDigit(character)
                ? char.ToUpperInvariant(character)
                : '_');
        }
        return result.ToString();
    }

    private string? readWindows(string name)
    {
        if (!CredRead(getTarget(name), CredentialTypeGeneric, 0, out IntPtr credentialPointer))
        {
            int error = Marshal.GetLastWin32Error();
            if (error == ErrorNotFound)
                return null;
            throw new Win32Exception(error);
        }
        try
        {
            NativeCredential credential =
                Marshal.PtrToStructure<NativeCredential>(credentialPointer);
            if (credential.CredentialBlob == IntPtr.Zero
                || credential.CredentialBlobSize == 0)
            {
                return string.Empty;
            }
            byte[] bytes = new byte[credential.CredentialBlobSize];
            Marshal.Copy(credential.CredentialBlob, bytes, 0, bytes.Length);
            return Encoding.Unicode.GetString(bytes);
        }
        finally
        {
            CredFree(credentialPointer);
        }
    }

    private void writeWindows(string name, string value)
    {
        byte[] bytes = Encoding.Unicode.GetBytes(value);
        IntPtr blob = Marshal.AllocCoTaskMem(bytes.Length);
        try
        {
            Marshal.Copy(bytes, 0, blob, bytes.Length);
            NativeCredential credential = new()
            {
                Type = CredentialTypeGeneric,
                TargetName = getTarget(name),
                CredentialBlobSize = (uint)bytes.Length,
                CredentialBlob = blob,
                Persist = CredentialPersistLocalMachine,
                UserName = pluginId,
            };
            if (!CredWrite(ref credential, 0))
                throw new Win32Exception(Marshal.GetLastWin32Error());
        }
        finally
        {
            byte[] zeros = new byte[bytes.Length];
            Marshal.Copy(zeros, 0, blob, zeros.Length);
            Marshal.FreeCoTaskMem(blob);
        }
    }

    private void deleteWindows(string name)
    {
        if (CredDelete(getTarget(name), CredentialTypeGeneric, 0))
            return;
        int error = Marshal.GetLastWin32Error();
        if (error != ErrorNotFound)
            throw new Win32Exception(error);
    }

    private async Task<string?> readMacOSAsync(
        string name,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        byte[] service = Encoding.UTF8.GetBytes(getTarget(name));
        byte[] account = Encoding.UTF8.GetBytes(pluginId);
        int status = SecKeychainFindGenericPassword(
            IntPtr.Zero,
            (uint)service.Length,
            service,
            (uint)account.Length,
            account,
            out uint passwordLength,
            out IntPtr passwordData,
            out IntPtr itemReference);
        if (status == MacOSItemNotFound)
            return await Task.FromResult<string?>(null);
        if (status != 0)
            throw new InvalidOperationException($"macOS Keychain read failed with status {status}.");
        try
        {
            byte[] password = new byte[passwordLength];
            try
            {
                Marshal.Copy(passwordData, password, 0, password.Length);
                return await Task.FromResult<string?>(Encoding.UTF8.GetString(password));
            }
            finally
            {
                CryptographicOperations.ZeroMemory(password);
            }
        }
        finally
        {
            SecKeychainItemFreeContent(IntPtr.Zero, passwordData);
            if (itemReference != IntPtr.Zero)
                CFRelease(itemReference);
        }
    }

    private async Task writeMacOSAsync(
        string name,
        string value,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        byte[] service = Encoding.UTF8.GetBytes(getTarget(name));
        byte[] account = Encoding.UTF8.GetBytes(pluginId);
        byte[] password = Encoding.UTF8.GetBytes(value);
        try
        {
            int findStatus = SecKeychainFindGenericPassword(
                IntPtr.Zero,
                (uint)service.Length,
                service,
                (uint)account.Length,
                account,
                out uint passwordLength,
                out IntPtr passwordData,
                out IntPtr itemReference);
            if (passwordData != IntPtr.Zero)
                SecKeychainItemFreeContent(IntPtr.Zero, passwordData);
            int status;
            if (findStatus == 0)
            {
                status = SecKeychainItemModifyAttributesAndData(
                    itemReference,
                    IntPtr.Zero,
                    (uint)password.Length,
                    password);
                if (itemReference != IntPtr.Zero)
                    CFRelease(itemReference);
            }
            else if (findStatus == MacOSItemNotFound)
            {
                status = SecKeychainAddGenericPassword(
                    IntPtr.Zero,
                    (uint)service.Length,
                    service,
                    (uint)account.Length,
                    account,
                    (uint)password.Length,
                    password,
                    out itemReference);
                if (itemReference != IntPtr.Zero)
                    CFRelease(itemReference);
            }
            else
            {
                throw new InvalidOperationException(
                    $"macOS Keychain lookup failed with status {findStatus}.");
            }
            if (status != 0)
                throw new InvalidOperationException($"macOS Keychain write failed with status {status}.");
            await Task.CompletedTask;
        }
        finally
        {
            CryptographicOperations.ZeroMemory(password);
        }
    }

    private async Task deleteMacOSAsync(
        string name,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        byte[] service = Encoding.UTF8.GetBytes(getTarget(name));
        byte[] account = Encoding.UTF8.GetBytes(pluginId);
        int findStatus = SecKeychainFindGenericPassword(
            IntPtr.Zero,
            (uint)service.Length,
            service,
            (uint)account.Length,
            account,
            out uint passwordLength,
            out IntPtr passwordData,
            out IntPtr itemReference);
        if (passwordData != IntPtr.Zero)
            SecKeychainItemFreeContent(IntPtr.Zero, passwordData);
        if (findStatus == MacOSItemNotFound)
            return;
        if (findStatus != 0)
            throw new InvalidOperationException(
                $"macOS Keychain lookup failed with status {findStatus}.");
        int status = SecKeychainItemDelete(itemReference);
        if (itemReference != IntPtr.Zero)
            CFRelease(itemReference);
        if (status != 0)
            throw new InvalidOperationException($"macOS Keychain delete failed with status {status}.");
        await Task.CompletedTask;
    }

    [DllImport("Advapi32.dll", EntryPoint = "CredReadW", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CredRead(
        string target,
        uint type,
        uint flags,
        out IntPtr credential);

    [DllImport("Advapi32.dll", EntryPoint = "CredWriteW", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CredWrite(
        ref NativeCredential credential,
        uint flags);

    [DllImport("Advapi32.dll", EntryPoint = "CredDeleteW", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CredDelete(
        string target,
        uint type,
        uint flags);

    [DllImport("Advapi32.dll")]
    private static extern void CredFree(IntPtr buffer);

    [DllImport(
        "/System/Library/Frameworks/Security.framework/Security",
        EntryPoint = "SecKeychainFindGenericPassword")]
    private static extern int SecKeychainFindGenericPassword(
        IntPtr keychain,
        uint serviceNameLength,
        byte[] serviceName,
        uint accountNameLength,
        byte[] accountName,
        out uint passwordLength,
        out IntPtr passwordData,
        out IntPtr itemReference);

    [DllImport(
        "/System/Library/Frameworks/Security.framework/Security",
        EntryPoint = "SecKeychainAddGenericPassword")]
    private static extern int SecKeychainAddGenericPassword(
        IntPtr keychain,
        uint serviceNameLength,
        byte[] serviceName,
        uint accountNameLength,
        byte[] accountName,
        uint passwordLength,
        byte[] passwordData,
        out IntPtr itemReference);

    [DllImport(
        "/System/Library/Frameworks/Security.framework/Security",
        EntryPoint = "SecKeychainItemModifyAttributesAndData")]
    private static extern int SecKeychainItemModifyAttributesAndData(
        IntPtr itemReference,
        IntPtr attributeList,
        uint length,
        byte[] data);

    [DllImport(
        "/System/Library/Frameworks/Security.framework/Security",
        EntryPoint = "SecKeychainItemDelete")]
    private static extern int SecKeychainItemDelete(IntPtr itemReference);

    [DllImport(
        "/System/Library/Frameworks/Security.framework/Security",
        EntryPoint = "SecKeychainItemFreeContent")]
    private static extern int SecKeychainItemFreeContent(
        IntPtr attributeList,
        IntPtr data);

    [DllImport(
        "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
        EntryPoint = "CFRelease")]
    private static extern void CFRelease(IntPtr value);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct NativeCredential
    {
        public uint Flags;
        public uint Type;
        public string TargetName;
        public string? Comment;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
        public uint CredentialBlobSize;
        public IntPtr CredentialBlob;
        public uint Persist;
        public uint AttributeCount;
        public IntPtr Attributes;
        public string? TargetAlias;
        public string UserName;
    }
}
