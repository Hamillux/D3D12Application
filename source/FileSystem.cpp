#include "FileSystem.h"
#include <shobjidl.h>
#include <limits>
#include <system_error>

#pragma comment(lib, "Ole32.lib")

namespace fs = std::filesystem;
using Path = FileSystem::Path;

namespace
{
	Path CaptureWritableRoot() noexcept
	{
		std::error_code ec;

		const Path current = fs::current_path(ec);

		if (ec || current.empty())
		{
			return {};
		}

		const Path canonical = fs::canonical(current, ec);

		if (ec)
		{
			return {};
		}

		return canonical.lexically_normal();
	}

	// FileSystem がロードされた時点のカレントディレクトリを
	// 書き込み可能ルートとして固定する。
	// 後から current_path() が変更されても書き込み範囲は広がらない。
	const Path WritableRoot = CaptureWritableRoot();

	bool PathComponentEquals(
		const Path& a,
		const Path& b) noexcept
	{
		// Windows の通常のパス比較に合わせて大文字小文字を無視する。
		return CompareStringOrdinal(
			a.c_str(), -1,
			b.c_str(), -1,
			TRUE) == CSTR_EQUAL;
	}

	bool IsSameOrDescendant(
		const Path& path,
		const Path& root) noexcept
	{
		auto pathIt = path.begin();
		auto rootIt = root.begin();

		for (; rootIt != root.end(); ++rootIt, ++pathIt)
		{
			if (pathIt == path.end() ||
				!PathComponentEquals(*pathIt, *rootIt))
			{
				return false;
			}
		}

		return true;
	}

	bool IsWriteAllowed(const Path& path) noexcept
	{
		if (WritableRoot.empty() || path.empty())
		{
			return false;
		}

		std::error_code ec;

		// absolute() で相対パスを現在位置に展開したうえで、
		// weakly_canonical() により既存のシンボリックリンクや
		// ジャンクション、および "." / ".." を解決する。
		const Path absolute = fs::absolute(path, ec);

		if (ec)
		{
			return false;
		}

		const Path canonical = fs::weakly_canonical(absolute, ec);

		if (ec)
		{
			// セキュリティ判定なので、解決失敗時は fail closed。
			return false;
		}

		return IsSameOrDescendant(
			canonical.lexically_normal(),
			WritableRoot);
	}

	bool HasWritePermission(fs::perms permissions) noexcept
	{
		constexpr auto WritePermissions =
			fs::perms::owner_write |
			fs::perms::group_write |
			fs::perms::others_write;

		return (permissions & WritePermissions) != fs::perms::none;
	}

	bool MatchesExtension(
		const Path& path,
		const Path& extension)
	{
		if (extension.empty())
		{
			return true;
		}

		return path.extension() == extension;
	}
}

// -----------------------------------------------------------------------------
// Dialog
// -----------------------------------------------------------------------------

Path FileSystem::OpenFileDialog(HWND owner)
{
	IFileOpenDialog* dialog = nullptr;

	HRESULT hr = CoCreateInstance(
		CLSID_FileOpenDialog,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&dialog));

	if (FAILED(hr))
	{
		return {};
	}

	Path result;

	if (SUCCEEDED(dialog->Show(owner)))
	{
		IShellItem* item = nullptr;

		if (SUCCEEDED(dialog->GetResult(&item)))
		{
			PWSTR filePath = nullptr;

			if (SUCCEEDED(
				item->GetDisplayName(SIGDN_FILESYSPATH, &filePath)))
			{
				result = Path(filePath);
				CoTaskMemFree(filePath);
			}

			item->Release();
		}
	}

	dialog->Release();
	return result;
}

// -----------------------------------------------------------------------------
// File
// -----------------------------------------------------------------------------

bool FileSystem::FileExists(const Path& path) noexcept
{
	std::error_code ec;
	return fs::is_regular_file(path, ec) && !ec;
}

std::int64_t FileSystem::FileSize(const Path& path) noexcept
{
	std::error_code ec;

	const std::uintmax_t size = fs::file_size(path, ec);

	if (ec)
	{
		return -1;
	}

	if (size >
		static_cast<std::uintmax_t>(
			std::numeric_limits<std::int64_t>::max()))
	{
		return -1;
	}

	return static_cast<std::int64_t>(size);
}

bool FileSystem::RemoveFile(const Path& path) noexcept
{
	if (!IsWriteAllowed(path))
	{
		return false;
	}

	if (!FileExists(path))
	{
		return false;
	}

	if (IsReadOnly(path))
	{
		return false;
	}

	std::error_code ec;
	const bool removed = fs::remove(path, ec);

	return removed && !ec;
}

bool FileSystem::IsReadOnly(const Path& path) noexcept
{
	std::error_code ec;

	const fs::file_status status = fs::status(path, ec);

	if (ec || !fs::exists(status))
	{
		return false;
	}

	// 実ファイルの属性だけでなく、FileSystem の書き込みポリシーも
	// 「読み取り専用」として公開する。
	if (!IsWriteAllowed(path))
	{
		return true;
	}

	if (status.permissions() == fs::perms::unknown)
	{
		return false;
	}

	return !HasWritePermission(status.permissions());
}

bool FileSystem::MoveFileTo(
	const Path& destination,
	const Path& source) noexcept
{
	// Move は destination の作成だけでなく source の削除も行うため、
	// 両方が書き込み可能ルート配下である必要がある。
	if (!IsWriteAllowed(destination) ||
		!IsWriteAllowed(source))
	{
		return false;
	}

	if (!FileExists(source))
	{
		return false;
	}

	{
		std::error_code ec;

		if (fs::exists(destination, ec))
		{
			return false;
		}
	}

	// まず rename を試す。
	// 同一ファイルシステム内なら通常これで完了する。
	{
		std::error_code ec;
		fs::rename(source, destination, ec);

		if (!ec)
		{
			return true;
		}
	}

	// rename が失敗した場合、
	// 別ボリュームへの移動である可能性もあるので
	// copy + delete にフォールバック。
	{
		std::error_code ec;

		if (!fs::copy_file(
			source,
			destination,
			fs::copy_options::none,
			ec))
		{
			return false;
		}

		ec.clear();

		if (!fs::remove(source, ec) || ec)
		{
			// Move として成立しなかったので可能なら rollback。
			std::error_code rollbackError;
			fs::remove(destination, rollbackError);

			return false;
		}
	}

	return true;
}

bool FileSystem::CopyFileTo(
	const Path& destination,
	const Path& source,
	bool overwrite) noexcept
{
	if (!IsWriteAllowed(destination))
	{
		return false;
	}

	if (!FileExists(source))
	{
		return false;
	}

	std::error_code ec;

	const fs::copy_options options =
		overwrite
		? fs::copy_options::overwrite_existing
		: fs::copy_options::none;

	const bool copied =
		fs::copy_file(
			source,
			destination,
			options,
			ec);

	return copied && !ec;
}

bool FileSystem::SetReadOnly(
	const Path& path,
	bool readOnly) noexcept
{
	if (!IsWriteAllowed(path))
	{
		return false;
	}

	std::error_code ec;

	if (!fs::exists(path, ec) || ec)
	{
		return false;
	}

	constexpr auto WritePermissions =
		fs::perms::owner_write |
		fs::perms::group_write |
		fs::perms::others_write;

	fs::permissions(
		path,
		WritePermissions,
		readOnly
		? fs::perm_options::remove
		: fs::perm_options::add,
		ec);

	return !ec;
}

std::optional<FileSystem::FileStat>
FileSystem::GetStatData(const Path& path) noexcept
{
	std::error_code ec;

	const fs::file_status symlinkStatus =
		fs::symlink_status(path, ec);

	if (ec || !fs::exists(symlinkStatus))
	{
		return std::nullopt;
	}

	FileStat result;
	result.IsSymlink = fs::is_symlink(symlinkStatus);

	const fs::file_status status =
		fs::status(path, ec);

	if (ec)
	{
		return std::nullopt;
	}

	result.IsDirectory =
		fs::is_directory(status);

	result.IsRegularFile =
		fs::is_regular_file(status);

	result.IsReadOnly =
		!IsWriteAllowed(path) ||
		(status.permissions() != fs::perms::unknown &&
			!HasWritePermission(status.permissions()));

	if (result.IsRegularFile)
	{
		const auto size = fs::file_size(path, ec);

		if (!ec)
		{
			result.Size = size;
		}

		ec.clear();
	}

	const auto time =
		fs::last_write_time(path, ec);

	if (!ec)
	{
		result.LastWriteTime = time;
	}

	return result;
}

std::optional<FileSystem::FileTime>
FileSystem::GetTimeStamp(const Path& path) noexcept
{
	std::error_code ec;

	const FileTime time =
		fs::last_write_time(path, ec);

	if (ec)
	{
		return std::nullopt;
	}

	return time;
}

bool FileSystem::SetTimeStamp(
	const Path& path,
	FileTime time) noexcept
{
	if (!IsWriteAllowed(path))
	{
		return false;
	}

	std::error_code ec;

	fs::last_write_time(
		path,
		time,
		ec);

	return !ec;
}

// -----------------------------------------------------------------------------
// Stream
// -----------------------------------------------------------------------------

std::unique_ptr<std::fstream>
FileSystem::OpenRead(
	const Path& path,
	bool allowWrite)
{
	if (allowWrite && !IsWriteAllowed(path))
	{
		return nullptr;
	}

	auto stream =
		std::make_unique<std::fstream>();

	std::ios::openmode mode =
		std::ios::binary |
		std::ios::in;

	if (allowWrite)
	{
		mode |= std::ios::out;
	}

	stream->open(path, mode);

	if (!stream->is_open())
	{
		return nullptr;
	}

	return stream;
}

std::unique_ptr<std::fstream>
FileSystem::OpenWrite(
	const Path& path,
	bool append,
	bool allowRead)
{
	if (!IsWriteAllowed(path))
	{
		return nullptr;
	}

	auto stream =
		std::make_unique<std::fstream>();

	std::ios::openmode mode =
		std::ios::binary |
		std::ios::out;

	if (allowRead)
	{
		mode |= std::ios::in;
	}

	if (append)
	{
		mode |= std::ios::app;
	}
	else
	{
		mode |= std::ios::trunc;
	}

	stream->open(path, mode);

	if (!stream->is_open())
	{
		return nullptr;
	}

	return stream;
}

// -----------------------------------------------------------------------------
// Directory
// -----------------------------------------------------------------------------

bool FileSystem::DirectoryExists(
	const Path& path) noexcept
{
	std::error_code ec;

	return
		fs::is_directory(path, ec) &&
		!ec;
}

bool FileSystem::MakeDirectory(
	const Path& path) noexcept
{
	if (!IsWriteAllowed(path))
	{
		return false;
	}

	if (DirectoryExists(path))
	{
		return true;
	}

	std::error_code ec;
	const bool created =
		fs::create_directory(path, ec);

	return created && !ec;
}

bool FileSystem::CreateDirectoryTree(
	const Path& path) noexcept
{
	if (!IsWriteAllowed(path))
	{
		return false;
	}

	if (DirectoryExists(path))
	{
		return true;
	}

	std::error_code ec;
	const bool created =
		fs::create_directories(path, ec);

	return created && !ec;
}

bool FileSystem::DeleteDirectory(
	const Path& path) noexcept
{
	if (!IsWriteAllowed(path))
	{
		return false;
	}

	std::error_code ec;

	if (!fs::exists(path, ec))
	{
		return !ec;
	}

	if (ec || !fs::is_directory(path, ec))
	{
		return false;
	}

	ec.clear();

	const bool removed =
		fs::remove(path, ec);

	return removed && !ec;
}

bool FileSystem::DeleteDirectoryRecursively(
	const Path& path) noexcept
{
	if (!IsWriteAllowed(path))
	{
		return false;
	}

	std::error_code ec;

	if (!fs::exists(path, ec))
	{
		return !ec;
	}

	if (ec || !fs::is_directory(path, ec))
	{
		return false;
	}

	ec.clear();
	fs::remove_all(path, ec);

	return !ec && !fs::exists(path);
}

bool FileSystem::CopyDirectoryTree(
	const Path& destination,
	const Path& source,
	bool overwrite) noexcept
{
	if (!IsWriteAllowed(destination))
	{
		return false;
	}

	if (!DirectoryExists(source))
	{
		return false;
	}

	std::error_code ec;

	if (fs::exists(destination, ec) &&
		!fs::is_directory(destination, ec))
	{
		return false;
	}

	fs::copy_options options =
		fs::copy_options::recursive |
		fs::copy_options::skip_symlinks;

	if (overwrite)
	{
		options |=
			fs::copy_options::overwrite_existing;
	}

	ec.clear();

	fs::copy(
		source,
		destination,
		options,
		ec);

	return !ec;
}

// -----------------------------------------------------------------------------
// Directory iteration
// -----------------------------------------------------------------------------

bool FileSystem::IterateDirectory(
	const Path& directory,
	const DirectoryVisitor& visitor)
{
	if (!visitor || !DirectoryExists(directory))
	{
		return false;
	}

	std::error_code ec;

	fs::directory_iterator iterator(
		directory,
		fs::directory_options::skip_permission_denied,
		ec);

	const fs::directory_iterator end;

	if (ec)
	{
		return false;
	}

	while (iterator != end)
	{
		const fs::directory_entry& entry = *iterator;

		std::error_code typeError;
		const bool isDirectory =
			entry.is_directory(typeError);

		if (typeError)
		{
			return false;
		}

		if (!visitor(entry.path(), isDirectory))
		{
			return false;
		}

		iterator.increment(ec);

		if (ec)
		{
			return false;
		}
	}

	return true;
}

bool FileSystem::IterateDirectoryRecursively(
	const Path& directory,
	const DirectoryVisitor& visitor)
{
	if (!visitor || !DirectoryExists(directory))
	{
		return false;
	}

	std::error_code ec;

	fs::recursive_directory_iterator iterator(
		directory,
		fs::directory_options::skip_permission_denied,
		ec);

	const fs::recursive_directory_iterator end;

	if (ec)
	{
		return false;
	}

	while (iterator != end)
	{
		const fs::directory_entry& entry = *iterator;

		std::error_code typeError;
		const bool isDirectory =
			entry.is_directory(typeError);

		if (typeError)
		{
			return false;
		}

		if (!visitor(entry.path(), isDirectory))
		{
			return false;
		}

		iterator.increment(ec);

		if (ec)
		{
			return false;
		}
	}

	return true;
}

bool FileSystem::IterateDirectoryStat(
	const Path& directory,
	const DirectoryStatVisitor& visitor)
{
	if (!visitor)
	{
		return false;
	}

	return IterateDirectory(
		directory,
		[&visitor](const Path& path, bool)
	{
		const auto stat =
			GetStatData(path);

		if (!stat)
		{
			return false;
		}

		return visitor(path, *stat);
	});
}

bool FileSystem::IterateDirectoryStatRecursively(
	const Path& directory,
	const DirectoryStatVisitor& visitor)
{
	if (!visitor)
	{
		return false;
	}

	return IterateDirectoryRecursively(
		directory,
		[&visitor](const Path& path, bool)
	{
		const auto stat =
			GetStatData(path);

		if (!stat)
		{
			return false;
		}

		return visitor(path, *stat);
	});
}

// -----------------------------------------------------------------------------
// Find
// -----------------------------------------------------------------------------

std::vector<Path>
FileSystem::FindFiles(
	const Path& directory,
	const Path& extension)
{
	std::vector<Path> files;

	IterateDirectory(
		directory,
		[&files, &extension](
			const Path& path,
			bool isDirectory)
	{
		if (!isDirectory &&
			MatchesExtension(path, extension))
		{
			files.emplace_back(path);
		}

		return true;
	});

	return files;
}

std::vector<Path>
FileSystem::FindFilesRecursively(
	const Path& directory,
	const Path& extension)
{
	std::vector<Path> files;

	IterateDirectoryRecursively(
		directory,
		[&files, &extension](
			const Path& path,
			bool isDirectory)
	{
		if (!isDirectory &&
			MatchesExtension(path, extension))
		{
			files.emplace_back(path);
		}

		return true;
	});

	return files;
}

// -----------------------------------------------------------------------------
// Path
// -----------------------------------------------------------------------------

Path FileSystem::ToAbsolutePath(
	const Path& path) noexcept
{
	std::error_code ec;

	Path result =
		fs::absolute(path, ec);

	if (ec)
	{
		return {};
	}

	return result;
}

Path FileSystem::NormalizePath(
	const Path& path) noexcept
{
	std::error_code ec;

	Path result =
		fs::weakly_canonical(path, ec);

	if (!ec)
	{
		return result;
	}

	// 対象がまだ存在していない場合などでも、
	// "." / ".." 程度は整理できる。
	return path.lexically_normal();
}
