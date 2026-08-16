#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <cstdint>
#include <Windows.h>


namespace FileSystem
{
	// 書き込み系 API は、このモジュールがロードされた時点の
	// カレントディレクトリ配下にのみ変更を許可する。
	// それ以外のパスは読み取り専用として扱う。
	using Path = std::filesystem::path;
	using FileTime = std::filesystem::file_time_type;

	struct FileStat
	{
		bool IsDirectory = false;
		bool IsRegularFile = false;
		bool IsSymlink = false;
		bool IsReadOnly = false;

		std::uintmax_t Size = 0;
		FileTime LastWriteTime{};
	};

	using DirectoryVisitor =
		std::function<bool(const Path& path, bool isDirectory)>;

	using DirectoryStatVisitor =
		std::function<bool(const Path& path, const FileStat& stat)>;

	// ---------------------------------------------------------------------
	// Dialog
	// ---------------------------------------------------------------------

	// キャンセルまたは失敗時は空 Path を返す。
	Path OpenFileDialog(HWND owner);

	// ---------------------------------------------------------------------
	// File
	// ---------------------------------------------------------------------

	[[nodiscard]]
	bool FileExists(const Path& path) noexcept;

	// ファイルが存在しない、またはサイズ取得失敗時は -1。
	[[nodiscard]]
	std::int64_t FileSize(const Path& path) noexcept;

	// 読み取り専用ファイルは削除しない。
	bool RemoveFile(const Path& path) noexcept;

	// OS の属性に加え、書き込み可能ルート外も true。
	[[nodiscard]]
	bool IsReadOnly(const Path& path) noexcept;

	// destination が既に存在する場合は失敗する。
	bool MoveFileTo(
		const Path& destination,
		const Path& source) noexcept;

	// overwrite == false かつ destination が存在する場合は失敗する。
	bool CopyFileTo(
		const Path& destination,
		const Path& source,
		bool overwrite = false) noexcept;

	bool SetReadOnly(
		const Path& path,
		bool readOnly) noexcept;

	[[nodiscard]]
	std::optional<FileStat>
		GetStatData(const Path& path) noexcept;

	[[nodiscard]]
	std::optional<FileTime>
		GetTimeStamp(const Path& path) noexcept;

	bool SetTimeStamp(
		const Path& path,
		FileTime time) noexcept;

	// ---------------------------------------------------------------------
	// File stream
	// ---------------------------------------------------------------------

	// allowWrite == true の場合は読み書き両方可能な stream を返す。
	[[nodiscard]]
	std::unique_ptr<std::fstream>
		OpenRead(
			const Path& path,
			bool allowWrite = false);

	// append == false の場合、既存内容を切り詰める。
	[[nodiscard]]
	std::unique_ptr<std::fstream>
		OpenWrite(
			const Path& path,
			bool append = false,
			bool allowRead = false);

	// ---------------------------------------------------------------------
	// Directory
	// ---------------------------------------------------------------------

	[[nodiscard]]
	bool DirectoryExists(const Path& path) noexcept;

	// 1階層のみ作成。
	// 既にディレクトリが存在している場合も true。
	bool MakeDirectory(const Path& path) noexcept;

	// 親ディレクトリを含めて作成。
	bool CreateDirectoryTree(const Path& path) noexcept;

	// 空ディレクトリのみ削除。
	// 既に存在しない場合も true。
	bool DeleteDirectory(const Path& path) noexcept;

	bool DeleteDirectoryRecursively(const Path& path) noexcept;

	bool CopyDirectoryTree(
		const Path& destination,
		const Path& source,
		bool overwrite = false) noexcept;

	// visitor が false を返すと走査を中断。
	bool IterateDirectory(
		const Path& directory,
		const DirectoryVisitor& visitor);

	bool IterateDirectoryRecursively(
		const Path& directory,
		const DirectoryVisitor& visitor);

	bool IterateDirectoryStat(
		const Path& directory,
		const DirectoryStatVisitor& visitor);

	bool IterateDirectoryStatRecursively(
		const Path& directory,
		const DirectoryStatVisitor& visitor);

	// extension は ".png" のように '.' を含める。
	// 空なら全ファイル。
	[[nodiscard]]
	std::vector<Path> FindFiles(
		const Path& directory,
		const Path& extension = {});

	[[nodiscard]]
	std::vector<Path> FindFilesRecursively(
		const Path& directory,
		const Path& extension = {});

	// ---------------------------------------------------------------------
	// Path
	// ---------------------------------------------------------------------

	[[nodiscard]]
	Path ToAbsolutePath(const Path& path) noexcept;

	[[nodiscard]]
	Path NormalizePath(const Path& path) noexcept;
}
