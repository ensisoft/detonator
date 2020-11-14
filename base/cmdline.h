 // Copyright (c) 2016 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "base/platform.h"

#pragma once

#include <string>
#include <typeinfo>
#include <stdexcept>
#include <memory>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cassert>

// simplistic command line parser
//
// How to deal with string arguments with spaces ?
// TLDR; In your terminal either pass:
//
//  "--foobar=some string"
//
//           or
//
//  --foobar=\"foo bar\”
//
// Longer explanation. The argument parsing code assumes
// that whatever should be considered as a single string
// is passed as a single string. So therefore it's up to
// the code doing the invocation to figure out how to
// quote string argument with spaces so that they arrive to
// your program's main function as a single entry in
// the argv array.

namespace base
{
    namespace detail {
        template<typename T>
        T FromString(const char* str)
        {
            std::stringstream ss;
            ss << str;
            T value;
            ss >> value;
            if (ss.fail())
                throw std::runtime_error(std::string("Can't interpret '") +  str + "' as wanted value type.");
            return value;
        }
        template<>
        std::string FromString<std::string>(const char* str)
        { return str; }
    } // detail

    // CommandLineArgumentStack initialized from a set of arguments given as a
    // an array of char* pointers and length of array.
    class CommandLineArgumentStack
    {
    public:
        // Initialize the command line from the given char* array
        // and the length of the array.  This will copy the data.
        // so it's safe to discard the array after the constructor
        // returns.
        CommandLineArgumentStack(int count, const char* argv[])
        {
            for (int i=0; i<count; ++i)
            {
                std::string tmp(argv[i]);
                const auto pos = tmp.find_first_of('=');
                if (pos == std::string::npos)
                {
                    m_argv.push_back(std::move(tmp));
                    continue;
                }
                auto key = tmp.substr(0, pos);
                auto val = tmp.substr(pos+1);
                m_argv.push_back(std::move(key));
                if (!val.empty())
                    m_argv.push_back(std::move(val));
            }
        }
        // Get the current argument.
        const char* GetCurrent() const
        { return m_argv[m_pos].c_str(); }
        // Returns true if there are still arguments available.
        bool HasNext() const
        { return m_pos < m_argv.size(); }
        // Pop the current argument.
        void Pop()
        { ++m_pos; }
        // Returns true if the current argument matches the given name.
        bool IsMatch(const std::string& name) const
        { return name == GetCurrent(); }
    private:
        std::size_t m_pos = 0;
        std::vector<std::string> m_argv;
    };

    // Helper function to create a command lin argument stack
    // based on the standard arguments given to the main function.
    // Assumes that argv[0] is the program name and this is skipped.
    // The arguments are copied and after the function returns
    // the argv array may be discarded.
    inline CommandLineArgumentStack CreateStandardArgs(int argc, char* argv[])
    {
        return CommandLineArgumentStack(argc-1, (const char**)&argv[1]);
    }

    // Interface for argument processing.
    class CommandLineArg
    {
    public:
        virtual ~CommandLineArg() = default;
        // Try to accept the current argument and consume the expected
        // number of associated values from the command line stack.
        // Returns true if successful or false if the command line stack
        // didn't contain the expected data.
        virtual bool Accept(CommandLineArgumentStack& cmd) = 0;
        // Checks whether this argument object matches the given name.
        virtual bool IsMatch(const std::string& name) const = 0;
        // Returns true if the argument was matched when the command line
        // args were parsed. Returns false which means that the arguments data
        // didn't contain anything for this particular arg.
        virtual bool WasMatched() const = 0;
        // Get the argument's name as it it's to be given on the command line.
        virtual const std::string& GetName() const = 0;
        // Get the user readable help string associated with this argument.
        virtual const std::string& GetHelp() const = 0;

        // Get the value of the argument converted into some type.
        template<typename T>
        operator const T& ()const
        {
            const void* ptr = this->retrieve(typeid(T));
            assert(ptr != nullptr && "Wrong type for command line argument.");
            return *static_cast<const T*>(ptr);
        }

        // Get the value of the argument converted into some type.
        template<typename T>
        const T& As() const
        {
            const void* ptr = this->retrieve(typeid(T));
            assert(ptr != nullptr && "Wrong type for command line argument.");
            return *static_cast<const T*>(ptr);
        }

    protected:
        virtual const void* retrieve(const std::type_info& matching_type) const = 0;

    private:
    };

    // Command line (on/off) flag. I.e. something doesn't expect any value.
    // For example --enable-foo
    class CommandLineFlag : public CommandLineArg
    {
    public:
        explicit
        CommandLineFlag(const std::string& flag_name, const std::string& flag_help)
        : m_name(flag_name), m_help(flag_help), m_value(false)
        {}

        virtual bool Accept(CommandLineArgumentStack& cmd) override
        {
            if (!cmd.IsMatch(m_name))
                return false;

            cmd.Pop();
            m_value = true;
            m_match = true;
            return true;
        }
        virtual bool IsMatch(const std::string& name) const override
        { return name == m_name; }
        virtual bool WasMatched() const override
        { return m_match; }
        virtual const std::string& GetName() const override
        { return m_name; }
        virtual const std::string& GetHelp() const override
        { return m_help; }

        // Returns true if the flag is set after the arguments have been parsed.
        bool IsSet() const
        { return m_value; }

    protected:
        virtual const void* retrieve(const std::type_info& matching_type) const override
        {
            if (typeid(m_value) == matching_type)
                return &m_value;
            return nullptr;
        }

    private:
        std::string m_name;
        std::string m_help;
        bool m_value   = false;
        bool m_match   = false;
    };

    // "--something value" kind of argument
    template<typename T>
    class CommandLineValue : public CommandLineArg
    {
    public:
        explicit
        CommandLineValue(const std::string& name, const std::string& help, const T& initial)
        : m_name(name), m_help(help), m_value(initial)
        {}

        virtual bool Accept(CommandLineArgumentStack& cmd) override
        {
            if (!cmd.IsMatch(m_name))
                return false;

            cmd.Pop();
            // Returns true if there's still the next argument
            if (!cmd.HasNext())
                throw std::runtime_error("Missing value for argument: '" + m_name + "'");
            m_value = detail::FromString<T>(cmd.GetCurrent());
            m_match = true;
            cmd.Pop();
            return true;
        }
        virtual bool IsMatch(const std::string& name) const override
        { return name == m_name; }
        virtual bool WasMatched() const override
        { return m_match; }
        virtual const std::string& GetName() const override
        { return m_name; }
        virtual const std::string& GetHelp() const override
        { return m_help; }

    protected:
        virtual const void* retrieve(const std::type_info& matching_type) const override
        {
            if (typeid(m_value) == matching_type)
                return &m_value;
            return nullptr;
        }

    private:
        std::string m_name;
        std::string m_help;
        T m_value;
        bool m_match = false;
    };

    // CommandLineOptions provides the main interface for parsing
    // a set of command line arguments. This is a two step process.
    // First the command line options instance is created and configured
    // with the appropriate command line args that are expected and
    // matched again the actual command line args given by the user.
    class CommandLineOptions
    {
    public:
        void Add(std::unique_ptr<CommandLineArg> arg)
        { m_options_list.push_back(std::move(arg)); }

        // Add a new command line value to be recognized.
        template<typename T>
        void Add(const std::string& name, const std::string& help, T initial_value)
        {
            Add(std::make_unique<CommandLineValue<T>>(name, help, initial_value));
        }

        // Add a new command line flag to be recognized.
        void Add(const std::string& name, const std::string& help)
        {
            Add(std::make_unique<CommandLineFlag>(name, help));
        }

        // Consume the data from the command line argument stack
        // and try to match the actual command line parameters against
        // the ones that have been configured in this  command line options object.
        // Returns true if command line was consumed succesfully, otherwise false
        // and some argument could not be matched properly.
        // On unexpected input an exception is thrown.
        void Parse(CommandLineArgumentStack& cmd, bool allow_unexpected = false)
        {
            while (cmd.HasNext())
            {
                bool was_accepted = false;
                for (auto& opt : m_options_list)
                {
                    if (opt->Accept(cmd))
                    {
                        was_accepted = true;
                        break;
                    }
                }
                if (!was_accepted && !allow_unexpected)
                    throw std::logic_error(std::string("Unexpected argument: ") + cmd.GetCurrent());
                else if (!was_accepted)
                    cmd.Pop();

            }
        }
        void Parse(CommandLineArgumentStack&& cmd, bool allow_unexpected)
        {
            CommandLineArgumentStack temp = std::move(cmd);
            Parse(temp, allow_unexpected);
        }

        // Convenience wrapper for converting a parse failure into boolean
        // result for simplicity. if the error string pointer is non nullptr
        // then on error the error message is copied into this string.
        bool Parse(CommandLineArgumentStack& cmd, std::string* error)
        {
            try
            {
                Parse(cmd);
            }
            catch (const std::exception& e)
            {
                if (error) *error = e.what();
                return false;
            }
            return true;
        }
        bool Parse(CommandLineArgumentStack&& cmd, std::string* error)
        {
            CommandLineArgumentStack temp = std::move(cmd);
            return Parse(temp, error);
        }

        const CommandLineArg& Get(const char* name) const
        {
            auto it = std::find_if(std::begin(m_options_list), std::end(m_options_list),
               [name](const auto& arg) {
                   return arg->IsMatch(name);
               });
            assert(it != std::end(m_options_list) && "No such argument");
            return *(*it);
        }

        // Checks whether the argument identified by name was actually given
        // in the list of command line arguments.
        bool WasGiven(const char* name) const
        {
            for (const auto& opt : m_options_list)
            {
                if (opt->IsMatch(name) && opt->WasMatched())
                    return true;
            }
            return false;
        }

        template<typename T>
        T GetValue(const char* name) const
        {
            for (const auto& opt : m_options_list)
            {
                if (opt->IsMatch(name))
                    return opt->As<T>();
            }
            assert(!"No such argument.");
            return T();
        }
        template<typename T>
        bool GetValue(const char* name, T* out) const
        {
            for (const auto& opt : m_options_list)
            {
                if (!opt->IsMatch(name))
                    continue;
                if (opt->WasMatched())
                {
                    *out = opt->As<T>();
                    return true;
                }
                return false;
            }
            assert(!"No such argument.");
            return false;
        }

        void Print(std::ostream& out) const
        {
            size_t longest_switch = 0;
            for (const auto& arg : m_options_list)
            {
                const auto& str = arg->GetName();
                longest_switch  = std::max(longest_switch, str.size());
            }
            for (const auto& arg : m_options_list)
            {
                const auto& name = arg->GetName();
                const auto& help = arg->GetHelp();
                out << name << std::setw(longest_switch + 1 - name.size()) << std::setfill(' ') << " ";
                out << "\t";
                out << help;
                out << std::endl;
            }
        }
    private:
        using list = std::vector<std::unique_ptr<CommandLineArg>>;
        list m_options_list;
    };

} // base
