#include "stdafx.h"

#include "Remaps.h"


// Factory class for outputs
class OutputFactory
{
private:

	// Prototypes, so that INPUTs don't need to be manually set everywhere

	INPUT UnicodePrototypeDown;
	INPUT UnicodePrototypeUp;

	INPUT VirtualKeyPrototypeDown;
	INPUT VirtualKeyPrototypeUp;

public:

	OutputFactory()
	{
		// Initialize prototypes

		// When keyeventf_unicode is set, virtual key must be 0,
		// and the UTF-16 code value is put into wScan
		// Surrogate pairs require two consecutive inputs
		UnicodePrototypeDown.type = INPUT_KEYBOARD;
		UnicodePrototypeDown.ki.dwExtraInfo = 0;
		UnicodePrototypeDown.ki.dwFlags = KEYEVENTF_UNICODE;
		UnicodePrototypeDown.ki.time = 0;
		UnicodePrototypeDown.ki.wVk = 0;

		UnicodePrototypeUp = UnicodePrototypeDown;
		UnicodePrototypeUp.ki.dwFlags |= KEYEVENTF_KEYUP;

		// Virtual keys are sent with the scancode e0 00 because the hook
		// will filter these out (to avoid responding to injected keys)
		VirtualKeyPrototypeDown.type = INPUT_KEYBOARD;
		VirtualKeyPrototypeDown.ki.dwExtraInfo = 0;
		VirtualKeyPrototypeDown.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
		VirtualKeyPrototypeDown.ki.time = 0;
		VirtualKeyPrototypeDown.ki.wScan = 0;

		VirtualKeyPrototypeUp = VirtualKeyPrototypeDown;
		VirtualKeyPrototypeUp.ki.dwFlags |= KEYEVENTF_KEYUP;
	}


	// ----USAGE----
	// The meaning of _nParam and _lParam change depending on _type:
	// type UnicodeOutput:
	//		_nParam contains the Unicode code point to be sent,
	//		_lParam is not used
	// type VirtualOutput:
	//		_nParam contains the virtual-key code to be simulated,
	//		_lParam contains modifier flags (up to eight) to be simulated as well
	// type MacroOutput:
	//		_lParam is a pointer to an array of DWORDs, each containing the virtual-key code
	//				to be sent, with the high bit (leftmost) on for keyups
	//		_nParam is the length of the array
	// type StringOutput:
	//		_lParam is a pointer to an array of UINTs, each containing the Unicode codepoint
	//				of the character to be sent
	//		_nParam is the length of the array
	// type ScriptOutput:
	//		_lParam is a pointer to a null-terminated string of WCHAR (WCHAR*) containing
	//				the full path to the file to execute
	//		_nParam is unused
	// type NoOutput uses no input, and generates a structure that performs no action when executed.
	IKeystrokeOutput * getInstance(KeystrokeOutputType _type, UINT _nParam, ULONG_PTR _lParam)
	{
		switch (_type)
		{
		case KeystrokeOutputType::UnicodeOutput:
		{
			// integer parameter contains the Unicode codepoint
			// second parameter contains nothing
			UnicodeOutput * unicodeOutput = new UnicodeOutput();
			UINT codepoint = _nParam;
			unicodeOutput->codepoint = codepoint;

			if (codepoint <= 0xffff)
			{
				// One UTF-16 code unit

				unicodeOutput->inputCount = 1;

				unicodeOutput->keystrokesDown = new INPUT(UnicodePrototypeDown);
				unicodeOutput->keystrokesDown->ki.wScan = codepoint;

				unicodeOutput->keystrokesUp = new INPUT(UnicodePrototypeUp);
				unicodeOutput->keystrokesUp->ki.wScan = codepoint;

				return (IKeystrokeOutput*)unicodeOutput;
			}
			else
			{
				// UTF-16 surrogate pair

				unicodeOutput->inputCount = 2;

				unicodeOutput->keystrokesDown = new INPUT[2];
				unicodeOutput->keystrokesUp = new INPUT[2];
				for (int i = 0; i < 2; i++) {
					unicodeOutput->keystrokesDown[i] = INPUT(UnicodePrototypeDown);
					unicodeOutput->keystrokesUp[i] = INPUT(UnicodePrototypeUp);
				}

				codepoint -= 0x10000;
				unicodeOutput->keystrokesDown[0].ki.wScan
					= unicodeOutput->keystrokesUp[0].ki.wScan
					= 0xd800 + (codepoint >> 10);			// High surrogate
				unicodeOutput->keystrokesDown[1].ki.wScan
					= unicodeOutput->keystrokesUp[1].ki.wScan
					= 0xdc00 + (codepoint & 0x3ff);			// Low surrogate

				return (IKeystrokeOutput*)unicodeOutput;

			}

		}	// end case
		case KeystrokeOutputType::VirtualOutput:
		{
			// Integer parameter contains the virtual-key code
			// Second parameter contains flags for modifiers (only the last byte)
			// (These are not the modifiers that trigger a remap, but the ones used
			// for simulating shortcuts

			VirtualKeyOutput * virtualKeyOutput = new VirtualKeyOutput();

			// There are better ways to count the number of set bits
			// but come on
			USHORT modifierCount = 0;
			for (unsigned int i = 0; i < 8; i++)
				if ((_lParam >> i) & 1)
					modifierCount++;
			// Above code doesn't modify value of _lParam
			virtualKeyOutput->inputCount = modifierCount + 1;

			// We need one INPUT for each modifier, plus one for the key itself
			USHORT currentIndex = 0;
			virtualKeyOutput->keystrokesDown = new INPUT[modifierCount + 1];
			virtualKeyOutput->keystrokesUp = new INPUT[modifierCount + 1];
			for (int i = 0; i < virtualKeyOutput->inputCount; i++) {
				virtualKeyOutput->keystrokesDown[i] = INPUT(VirtualKeyPrototypeDown);
				virtualKeyOutput->keystrokesUp[i] = INPUT(VirtualKeyPrototypeUp);
			}

			// Careful - keystrokes up are in inverse order, with index 0 corresponding to the key itself
			{
				if ((_lParam & VIRTUAL_MODIFIER_LCTRL) == VIRTUAL_MODIFIER_LCTRL)
				{
					virtualKeyOutput->keystrokesDown[currentIndex].ki.wVk = VK_LCONTROL;
					virtualKeyOutput->keystrokesUp[modifierCount - currentIndex].ki.wVk = VK_LCONTROL;
					currentIndex++;
				}
				if ((_lParam & VIRTUAL_MODIFIER_RCTRL) == VIRTUAL_MODIFIER_RCTRL)
				{
					virtualKeyOutput->keystrokesDown[currentIndex].ki.wVk = VK_RCONTROL;
					virtualKeyOutput->keystrokesUp[modifierCount - currentIndex].ki.wVk = VK_RCONTROL;
					currentIndex++;
				}
				if ((_lParam & VIRTUAL_MODIFIER_LALT) == VIRTUAL_MODIFIER_LALT)
				{
					virtualKeyOutput->keystrokesDown[currentIndex].ki.wVk = VK_LMENU;
					virtualKeyOutput->keystrokesUp[modifierCount - currentIndex].ki.wVk = VK_LMENU;
					currentIndex++;
				}
				if ((_lParam & VIRTUAL_MODIFIER_RALT) == VIRTUAL_MODIFIER_RALT)
				{
					virtualKeyOutput->keystrokesDown[currentIndex].ki.wVk = VK_RMENU;
					virtualKeyOutput->keystrokesUp[modifierCount - currentIndex].ki.wVk = VK_RMENU;
					currentIndex++;
				}
				if ((_lParam & VIRTUAL_MODIFIER_LWIN) == VIRTUAL_MODIFIER_LWIN)
				{
					virtualKeyOutput->keystrokesDown[currentIndex].ki.wVk = VK_LWIN;
					virtualKeyOutput->keystrokesUp[modifierCount - currentIndex].ki.wVk = VK_LWIN;
					currentIndex++;
				}
				if ((_lParam & VIRTUAL_MODIFIER_RWIN) == VIRTUAL_MODIFIER_RWIN)
				{
					virtualKeyOutput->keystrokesDown[currentIndex].ki.wVk = VK_RWIN;
					virtualKeyOutput->keystrokesUp[modifierCount - currentIndex].ki.wVk = VK_RWIN;
					currentIndex++;
				}
				if ((_lParam & VIRTUAL_MODIFIER_LSHIFT) == VIRTUAL_MODIFIER_RSHIFT)
				{
					virtualKeyOutput->keystrokesDown[currentIndex].ki.wVk = VK_LSHIFT;
					virtualKeyOutput->keystrokesUp[modifierCount - currentIndex].ki.wVk = VK_LSHIFT;
					currentIndex++;
				}
				if ((_lParam & VIRTUAL_MODIFIER_RSHIFT) == VIRTUAL_MODIFIER_RSHIFT)
				{
					virtualKeyOutput->keystrokesDown[currentIndex].ki.wVk = VK_RSHIFT;
					virtualKeyOutput->keystrokesUp[modifierCount - currentIndex].ki.wVk = VK_RSHIFT;
					currentIndex++;
				}
			}
			virtualKeyOutput->keystrokesDown[currentIndex].ki.wVk = (WORD)_nParam;
			virtualKeyOutput->keystrokesUp[modifierCount - currentIndex].ki.wVk = (WORD)_nParam;

			return (IKeystrokeOutput*)virtualKeyOutput;

		}	// end case

		case KeystrokeOutputType::MacroOutput:
		{
			// second parameter casts to a pointer to an array of DWORD containing the sequence
			//		of keypresses (each DWORD contains the virtual key code in the last byte, and
			//		the high bit set for keyups)
			// first parameter contains the lenght of array pointed to by second parameter.
			if (_lParam == 0) return nullptr;

			// WORD: 16-bit
			// DWORD: 32-bit
			// I think the size of Microsoft words will stay that way until the nth end of the word.
			DWORD * sequence = (DWORD*)_lParam;
			UINT seqCount = _nParam;

			auto macroOutput = new MacroOutput();
			macroOutput->inputCount = seqCount;
			macroOutput->keystrokes = new INPUT[seqCount];

			BOOL keyup = 0;
			USHORT virtualKeyCode = 0;
			for (unsigned int i = 0; i < seqCount; i++)
			{
				keyup = (sequence[i] >> 31) & 1;
				virtualKeyCode = sequence[i] & 0xff;
				macroOutput->keystrokes[i] = INPUT(VirtualKeyPrototypeDown);
				macroOutput->keystrokes[i].ki.wVk = virtualKeyCode;
				if (keyup) macroOutput->keystrokes[i].ki.dwFlags |= KEYEVENTF_KEYUP;
			}
			return (IKeystrokeOutput*)macroOutput;

		}	// end case

		case KeystrokeOutputType::StringOutput:
		{
			// second parameter casts to a pointer to array of UINT containing all
			// the Unicode code points in string
			// first parameter contains the length of array pointed to by second parameter
			if (_lParam == 0) return nullptr;

			UINT * codepoints = (UINT*)_lParam;
			UINT seqCount = _nParam;

			auto stringOutput = new StringOutput();

			// number of sent INPUTs is the length of array, plus the amount of code points
			//		that are larger than 0xffff
			// Then multiplied by 2 because both keydowns and keyups are sent
			stringOutput->inputCount = seqCount * 2;
			for (unsigned int i = 0; i < seqCount; i++)
			{
				if (codepoints[i] > 0xffff)
					stringOutput->inputCount += 2;
			}

			stringOutput->keystrokes = new INPUT[stringOutput->inputCount];



			// Put each character and surrogate pair into array of inputs
			UINT currentIndex = 0;

			stringOutput->keystrokes = new INPUT[stringOutput->inputCount];

			for (unsigned int i = 0; i < seqCount; i++)
			{
				// codepoints = one codepoint per character
				// seqCount = number of characters
				// stringOutput->inputCount = number of UTF-16 code values
				// currentIndex must be incremented manually

				if (codepoints[i] <= 0xffff)
				{
					// one UTF-16 code value, one input down, one input up
					stringOutput->keystrokes[currentIndex] = INPUT(UnicodePrototypeDown);
					stringOutput->keystrokes[currentIndex].ki.wScan = codepoints[i];
					currentIndex++;
					stringOutput->keystrokes[currentIndex] = INPUT(UnicodePrototypeUp);
					stringOutput->keystrokes[currentIndex].ki.wScan = codepoints[i];
					currentIndex++;
					continue;		// next for
				}
				else
				{
					// UTF-16 surrogate pair, two inputs down, two inputs up
					USHORT highSurrogate = 0xd800 + ((codepoints[i] - 0x10000) >> 10);
					USHORT lowSurrogate = 0xdc00 + (codepoints[i] & 0x3ff);
					stringOutput->keystrokes[currentIndex] = INPUT(UnicodePrototypeDown);
					stringOutput->keystrokes[currentIndex + 1] = INPUT(UnicodePrototypeDown);
					stringOutput->keystrokes[currentIndex].ki.wScan = highSurrogate;
					stringOutput->keystrokes[currentIndex + 1].ki.wScan = lowSurrogate;
					currentIndex += 2;
					stringOutput->keystrokes[currentIndex] = INPUT(UnicodePrototypeUp);
					stringOutput->keystrokes[currentIndex + 1] = INPUT(UnicodePrototypeUp);
					stringOutput->keystrokes[currentIndex].ki.wScan = highSurrogate;
					stringOutput->keystrokes[currentIndex + 1].ki.wScan = lowSurrogate;
					currentIndex += 2;
					continue;
				}
			}	// end for
			return (IKeystrokeOutput*)stringOutput;

		}	// end case


		case KeystrokeOutputType::ScriptOutput:
		{
			// second parameter contains a pointer to null-terminated WCHAR string containing filename to executable
			// first parameter contains nothing

			ScriptOutput * scriptOutput = new ScriptOutput((WCHAR*)_lParam);
			return (IKeystrokeOutput*)scriptOutput;
		}	// end case

		
		case KeystrokeOutputType::NoOutput:
		{
			// No inputs
			NoOutput * noOutput = new NoOutput();
			return (IKeystrokeOutput*)noOutput;
		}	// end case


		default: return nullptr;
		}	// end switch


	}
};






































class Parser
{
public:


	static BOOL ReadSymbol(std::ifstream* stream, std::string* symbol)
	{
		int input_char = 0;		// buffer for one character
		symbol->clear();

		if (stream->peek() == -1)			// If the very next thing is the end
			return FALSE;					// then return false

		while (isalpha(stream->peek()))			// if the next character is alphabetic
		{
			(*symbol) += (char)stream->get();		// then add that character to the string
			if (stream->peek() == -1)				// if the next thing is an end-of-file,
				return TRUE;						// then we're done
		}											// and repeat

													// reaching this point means we've encountered a non-alphabetic character
		return TRUE;
	}








	static BOOL ReadKeyboardName(std::ifstream* stream, KEYBOARD* kb)
	{
		// from <codecvt>, the object that will convert between narrow and wide strings (C++11 and newer)
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		// Because we read in UTF-8, but device names are stored internally as WCHARs.

		auto buffer = std::string();					// utf-8
		auto wideBuffer = std::wstring();				// utf-16

		while (!isspace(stream->peek()) && stream->peek() != ')')
		{
			buffer += stream->get();
		}
		if (buffer == "") return FALSE;		// kb name wasn't there
		wideBuffer = converter.from_bytes(buffer);			// convert and then store it in the wide buffer
		swprintf_s(kb->device_name, kb->device_name_sizeof, L"%ls", wideBuffer.c_str());
		return TRUE;
	}






	static BOOL ReadModifierHex(std::ifstream* stream, BYTE* modifiers, UINT32* code)
	{
		return FALSE;
		/*
		int input_char = 0;		// buffer for one character

		int sideModifier = 0;	// 0: next modifier applies to left and right
								// 1: left modifier
								// 2: right modifier

		(*modifiers) = 0;		// clearing...
		(*code) = 0;

		input_char = stream->peek();		// evaluate *next* character
		while (true)
		{
			switch (input_char)					// TODO: Put this switch into a loop
			{
			case '<':					// next modifier is left only
				sideModifier = 1;
				stream->get();
				break;

			case '>':					// next modifier is right only
				sideModifier = 2;
				stream->get();
				break;

			case '#':					// Winkey (unaffected by side modifiers)
				if (sideModifier == 1 || sideModifier == 0)
					*modifiers |= MODIFIER_LWIN;
				if (sideModifier == 2 || sideModifier == 0)
					*modifiers |= MODIFIER_RWIN;		// (I've never seen a right Winkey)
				sideModifier = 0;
				stream->get();
				break;

			case '^':					// Ctrl
				if (sideModifier == 1 || sideModifier == 0)
					*modifiers |= MODIFIER_LCTRL;
				if (sideModifier == 2 || sideModifier == 0)
					*modifiers |= MODIFIER_RCTRL;
				sideModifier = 0;
				stream->get();
				break;

			case '!':					// Alt
				if (sideModifier == 1 || sideModifier == 0)
					*modifiers |= MODIFIER_LALT;
				if (sideModifier == 2 || sideModifier == 0)
					*modifiers |= MODIFIER_RALT;
				sideModifier = 0;
				stream->get();
				break;

			case '+':					// Shift
				if (sideModifier == 1 || sideModifier == 0)
					*modifiers |= MODIFIER_LSHIFT;
				if (sideModifier == 2 || sideModifier == 0)
					*modifiers |= MODIFIER_RSHIFT;
				sideModifier = 0;
				stream->get();
				break;

			case '0':
			{	// brackets for locality
				// expect an 'x' or 'X'
				stream->get();
				input_char = stream->peek();
				if (input_char != 'x' && input_char != 'X')
					return FALSE;
				stream->get();
				// while the next character is a hexadecimal digit, read it
				std::string numberBuffer = std::string();
				while (isxdigit(stream->peek()))
				{
					numberBuffer += stream->get();
				}
				if (sscanf_s(numberBuffer.c_str(), "%x", code) != 1)
					return FALSE;		// that's one way to cast
										// we're not trying to be fast, exactly
										// scanf returns the amount of variables scanned
				else return TRUE;
			}

			default:			// Do not extract character
				return FALSE;
			}	// end of switch
		}	// loop
		*/
	}













	static BOOL ReadInputKeystroke(std::ifstream* stream, KEYSTROKE_INPUT* trigger)
	{
		UINT32 code = 0;
		BOOL result = ReadModifierHex(stream, &(trigger->modifiers), &code);
		trigger->scancode = (USHORT)code;			// lossy cast, but shouldn't lose any data
		return result;
	}


	static BOOL ReadVirtualkey(std::ifstream* stream, IKeystrokeOutput* keystroke)
	{
		//keystroke->flags = 0;
		//return ReadModifierHex(stream, &(keystroke->modifiers), &(keystroke->codepoint));
	}


	static BOOL ReadUnicode(std::ifstream* stream, IKeystrokeOutput* keystroke)
	{
		/*
		keystroke->flags = KEYEVENTF_UNICODE;

		// All modifiers will be read but ignored
		while (stream->peek() == '<'
		|| stream->peek() == '>'
		|| stream->peek() == '#'
		|| stream->peek() == '^'
		|| stream->peek() == '!'
		|| stream->peek() == '+')
		stream->get();

		if (stream->peek() != '0')			// expect a hexadecimal
		return FALSE;
		stream->get();
		if (stream->peek() != 'x' && stream->peek() != 'X')
		return FALSE;
		stream->get();

		std::string numberBuffer = std::string();
		while (isxdigit(stream->peek()))		// read all following hex digits
		{
		numberBuffer += stream->get();
		}
		if (sscanf_s(numberBuffer.c_str(), "%x", &(keystroke->codepoint)) != 1)
		return FALSE;

		return TRUE;
		*/
	}




	static BOOL ReadFile(std::ifstream* stream, std::vector<KEYBOARD> * ptrVectorKeyboard)
	{
		// Make a dummy object for now to test other things

		// Have this file generate the data structure, and build the parser later.

		// Yeah, this is pretty much working as a test class
		// because we don't have a proper test setup
		// sorry
		ptrVectorKeyboard->clear();

		auto keyboard = KEYBOARD();

		keyboard.device_name = L"\\\\?\\HID#VID_0510&PID_0002#7&141e5925&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}";
		keyboard.remaps.clear();


		auto input = KEYSTROKE_INPUT();

		input.flags = 0;
		input.modifiers = 0;


		OutputFactory factory = OutputFactory();


		auto pointerToAnotherOutput = factory.getInstance(KeystrokeOutputType::UnicodeOutput, 0x1f605, NULL);
		
		input.scancode = 0x02;
		keyboard.remaps.insert(std::pair<KEYSTROKE_INPUT, IKeystrokeOutput*>(input, pointerToAnotherOutput));

		auto pointerToYetAnotherOutputButThisTimeItsAVirtualOutput =
			factory.getInstance(KeystrokeOutputType::VirtualOutput, 0x020, 0);
		input.scancode = 0x03;
		keyboard.remaps.insert(std::pair<KEYSTROKE_INPUT, IKeystrokeOutput*>
			(input, pointerToYetAnotherOutputButThisTimeItsAVirtualOutput));

		DWORD macro[4];
		macro[0] = VK_LCONTROL;
		macro[1] = 0x46;
		macro[2] = 0x46 | 0x80000000;
		macro[3] = VK_LCONTROL | 0x80000000;
		
		auto pointerThisOneIsAMacro = factory.getInstance(KeystrokeOutputType::MacroOutput, 4, (ULONG_PTR)&macro);

		input.scancode = 0x04;
		keyboard.remaps.insert(std::pair<KEYSTROKE_INPUT, IKeystrokeOutput*>(input, pointerThisOneIsAMacro));



		UINT unicodeString[4];
		unicodeString[0] = 0x48;
		unicodeString[1] = 0x69;
		unicodeString[2] = 0x2e;
		unicodeString[3] = 0x1f642;		// prints "Hi." and a smiling emoji.
		auto pointerStringOfUnicode = factory.getInstance(KeystrokeOutputType::StringOutput, 4, (ULONG_PTR)&unicodeString);

		input.scancode = 0x05;
		keyboard.remaps.insert(std::pair<KEYSTROKE_INPUT, IKeystrokeOutput*>(input, pointerStringOfUnicode));


		auto wideStringFilename = L"C:\\MultiKeys\\openAppTest.exe";
		auto pointerThisOneWillOpenAnExecutableWhichOpensChromeIDontCareIfEdgeIsFasterChromeIsStillBetter
			= factory.getInstance(KeystrokeOutputType::ScriptOutput, 0, (ULONG_PTR)wideStringFilename);
		input.scancode = 0x06;
		keyboard.remaps.insert(std::pair<KEYSTROKE_INPUT, IKeystrokeOutput*>
			(input, pointerThisOneWillOpenAnExecutableWhichOpensChromeIDontCareIfEdgeIsFasterChromeIsStillBetter));



		ptrVectorKeyboard->push_back(keyboard);

		return TRUE;

		/*
		// having a function explicitly expect such a specific map is not very nice, but that's what I could do.

		// Also, in the future we should consider supporting UTF-16 filenames. I don't know how to do that. // <- that's done

		setlocale(LC_ALL, "");						// Set the locale, just in case


		ptrVectorKeyboard->clear();					// clear all keyboards with the maps in them


		// hold one line
		auto lineBuffer = std::string();

		// hold one symbol
		auto symbol = std::string();

		// hold the current keyboard being written to
		auto keyboard = KEYBOARD();

		// input and output
		auto input = KEYSTROKE_INPUT();
		auto output = IKeystrokeOutput();

		// one character
		int read_char = 0;


		// First term should be a "keyboard"
		if (!ReadSymbol(stream, &symbol)) return FALSE;
		if (symbol != "keyboard") return FALSE;
		// expect left parenthesis
		read_char = stream->get();
		if (read_char != '(') return FALSE;
		// read keyboard name
		if (!ReadKeyboardName(stream, &keyboard)) return FALSE;
		// expect right parenthesis
		read_char = stream->get();
		if (read_char != ')') return FALSE;
		// jump to next line
		getline(*stream, lineBuffer);
		// begin loop
		while (true)
		{
		if (!ReadSymbol(stream, &symbol))
		{
		// save keyboard and close
		ptrVectorKeyboard->push_back(keyboard);
		return TRUE;
		}
		// evaluate symbol
		if (symbol == "unicode")
		{
		// case 1 - unicode
		// expect a left parenthesis
		read_char = stream->get();
		if (read_char != '(') return FALSE;
		// first parameter: an input keystroke
		if (!ReadInputKeystroke(stream, &input)) return FALSE;
		// expect a comma
		read_char = stream->get();
		if (read_char != ',') return FALSE;
		// second parameter: an output keystroke with a unicode codepoint
		if (!ReadUnicode(stream, &output)) return FALSE;
		// expect a right parenthesis
		read_char = stream->get();
		if (read_char != ')') return FALSE;
		// place input and output into current keyboard
		keyboard.remaps.insert((std::pair<KEYSTROKE_INPUT, IKeystrokeOutput>(input, output)));
		}
		else if (symbol == "virtual")
		{
		// case 2 - virtual key
		// expect a left parenthesis
		read_char = stream->get();
		if (read_char != '(') return FALSE;
		// first parameter: an input keystroke
		if (!ReadInputKeystroke(stream, &input)) return FALSE;
		// expect a comma
		read_char = stream->get();
		if (read_char != ')') return FALSE;
		// second parameter: an output keystroke with a virtual key
		if (!ReadVirtualkey(stream, &output)) return FALSE;
		// expect a right parenthesis
		read_char = stream->get();
		if (read_char != ')') return FALSE;
		// place input and output into current keyboard
		keyboard.remaps.insert((std::pair<KEYSTROKE_INPUT, IKeystrokeOutput>(input, output)));
		}
		else if (symbol == "keyboard")
		{
		// case 3 - new keyboard
		// save this keyboard
		ptrVectorKeyboard->push_back(keyboard);		// store a copy
		// expect left parenthesis
		read_char = stream->get();
		if (read_char != '(') return FALSE;
		// clean keyboard
		keyboard.Clear();
		// read keyboard name
		if (!ReadKeyboardName(stream, &keyboard)) return FALSE;
		// expect right parenthesis
		read_char = stream->get();
		if (read_char != ')') return FALSE;
		}
		// jump line
		getline(*stream, lineBuffer);

		}	// loop
		*/


	}



};






















/*
Multikeys::Remapper::Remapper(std::string filename)					// constructor
{
	// Setup a keyboard input simulator
	// inputSimulator = InputSimulator();

	if (!LoadSettings(filename))
	{
		
	}
	OutputDebugString(L"Initialized.");
}
Multikeys::Remapper::Remapper(std::wstring filename)
{
	// Setup a keyboard input simulator
	// inputSimulator = InputSimulator();

	if (!LoadSettings(filename))
	{ }
	OutputDebugString(L"Initialized.");
} */
Multikeys::Remapper::Remapper()
{
}




BOOL Multikeys::Remapper::LoadSettings(std::string filename)		// parser
{
	std::ifstream file(filename);		// Open file with ANSI filename

	if (!file.is_open())
		return FALSE;

	BOOL result = Parser::ReadFile(&file, &keyboards);
	file.close();
	return result;
}
BOOL Multikeys::Remapper::LoadSettings(std::wstring filename)
{
	std::ifstream file(filename);		// Open file with unicode (UTF-16) filename

	if (!file.is_open())
		return FALSE;

	BOOL result = Parser::ReadFile(&file, &keyboards);
	file.close();
	return result;
}







BOOL Multikeys::Remapper::EvaluateKey(RAWKEYBOARD* keypressed, WCHAR* deviceName, IKeystrokeOutput ** out_action)
{
	// Make an input

	// Need to deal with modifiers. Not here, the levels thing.
	KEYSTROKE_INPUT input = KEYSTROKE_INPUT(0, keypressed->MakeCode, keypressed->Flags);
	// We'll look for a similar one in our list
	
	// Look for correct device; return FALSE (= do not block) otherwise
	for (auto iterator = keyboards.begin(); iterator != keyboards.end(); iterator++)
	{
		if (wcscmp(iterator->device_name, deviceName) == 0)
		{
			// found it!
			// check if the remaps map for this device contains our scancode
			auto pairIterator = iterator->remaps.find(input);
			if (pairIterator != iterator->remaps.end())
			{
				*out_action = pairIterator->second;
				return TRUE;
			}
			else
				return FALSE;
			
		}
	}

	return FALSE;
}







