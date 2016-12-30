var wshShell = new ActiveXObject("WScript.Shell")
var oFS = new ActiveXObject("Scripting.FileSystemObject");

var outfile = "./scmrev.h";
var cmd_revision = " rev-parse HEAD";
var cmd_count = " rev-list --count HEAD ^e1656af8191700f32c18b06d18b9a099d281b95b";
var cmd_describe = " describe --always --long --dirty";
var cmd_branch = " rev-parse --abbrev-ref HEAD";
var cmd_lastmodified = " log --pretty=oneline  -n 1 -- ";

function GetGitExe()
{
	try {
		gitexe = wshShell.RegRead("HKCU\\Software\\GitExtensions\\gitcommand");
		wshShell.Exec(gitexe);
		return gitexe;
	}
	catch (e)
	{ }

	for (var gitexe in { "git.cmd": 1, "git": 1, "git.bat": 1 }) {
		try {
			wshShell.Exec(gitexe);
			return gitexe;
		}
		catch (e)
		{ }
	}

	WScript.Echo("Cannot find git or git.cmd, check your PATH:\n" +
		wshShell.ExpandEnvironmentStrings("%PATH%"));
	WScript.Quit(1);
}

function GetFirstStdOutLine(cmd)
{
	try {
		return wshShell.Exec(cmd).StdOut.ReadLine();
	}
	catch (e) {
		// catch "the system cannot find the file specified" error
		WScript.Echo("Failed to exec " + cmd + " this should never happen");
		WScript.Quit(1);
	}
}

function GetFileContents(f)
{
	try {
		return oFS.OpenTextFile(f).ReadAll();
	}
	catch (e) {
		// file doesn't exist
		return "";
	}
}

// get info from git
var gitexe = GetGitExe();
var revision = GetFirstStdOutLine(gitexe + cmd_revision);
var revcount = GetFirstStdOutLine(gitexe + cmd_count);
var describe = GetFirstStdOutLine(gitexe + cmd_describe);
var branch = GetFirstStdOutLine(gitexe + cmd_branch);
var isStable	= +("master" == branch || "stable" == branch);

// Get environment information.
var distributor = wshShell.ExpandEnvironmentStrings("%DOLPHIN_DISTRIBUTOR%");
if (distributor == "%DOLPHIN_DISTRIBUTOR%") distributor = "Ishiiruka-based";

// remove hash (and trailing "-0" if needed) from description
describe = describe.replace(/(-0)?-[^-]+(-dirty)?$/, '$2');

var out_contents =
	"#define SCM_REV_STR \"" + revision + "\"\n" +
	"#define SCM_DESC_STR \"" + revcount + " (" + describe + ")\"\n" +
	"#define SCM_BRANCH_STR \"" + branch + "\"\n" +
	"#define SCM_IS_MASTER " + isStable + "\n" +
	"#define SCM_DISTRIBUTOR_STR \"" + distributor + "\"\n";
// check if file needs updating
if (out_contents == GetFileContents(outfile))
{
	WScript.Echo(outfile + " current at " + describe);
}
else
{
	// needs updating - writeout current info
	oFS.CreateTextFile(outfile, true).Write(out_contents);
	WScript.Echo(outfile + " updated to " + describe);
}
