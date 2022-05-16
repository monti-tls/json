/* MIT License
 * 
 * Copyright 2015 - 2022 Alexandre Monti
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __JSON_H__
#define __JSON_H__

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <stdexcept>

#ifndef JSON_START_NAMESPACE
# define JSON_START_NAMESPACE namespace json {
#endif

#ifndef JSON_END_NAMESPACE
# define JSON_END_NAMESPACE }
#endif

#ifndef JSON_NAMESPACE
# define JSON_NAMESPACE(name) json::name
#endif

JSON_START_NAMESPACE

// --------------------------------------------------------------------------------------
// Forward declarations
// --------------------------------------------------------------------------------------

class Token;
class Lexer;
class NumberNode;
class BooleanNode;
class StringNode;
class ObjectNode;
class ArrayNode;
class Node;
class Parser;
class NodeError;
class Element;
class Template;

// --------------------------------------------------------------------------------------
// Utility template
// --------------------------------------------------------------------------------------

Node* parse(std::string const& file);
Node* parse(std::istream& file);

void serialize(Node* node, std::string const& file, bool indent = true);
void serialize(Node* node, std::ostream& file, bool indent = true);

void extract(Template const& tpl, std::string const& file);
void extract(Template const& tpl, std::istream& file);

void synthetize(Template const& tpl, std::string const& file, bool indent = true);
void synthetize(Template const& tpl, std::ostream& file, bool indent = true);

// --------------------------------------------------------------------------------------
// Token
// --------------------------------------------------------------------------------------

class Token
{
public:
    enum Type
    {
        Eof,
        Bad,
        LeftBrace,
        RightBrace,
        LeftBracket,
        RightBracket,
        Comma,
        Colon,
        True,
        False,
        Null,
        Number,
        String,
        Include
    };

    struct Info
    {
        bool empty;
        int line, column;
    };

public:
    Token(Type type = Bad, std::string const& value = "") :
        m_type(type),
        m_value(value)
    {
        m_info.empty = true;
    }

    Type type() const
    { return m_type; }

    std::string const& value() const
    { return m_value; }

    void setInfo(Info const& info)
    {
        m_info = info;
        m_info.empty = false;
    }

    Info const& info() const
    { return m_info; }

private:
    Type m_type;
    std::string m_value;
    Info m_info;
};

// --------------------------------------------------------------------------------------
// Exception classes
// --------------------------------------------------------------------------------------

class TokenError : public std::runtime_error
{
public:
    TokenError(Token const& token, std::string const& what) :
        std::runtime_error(what),
        m_token(token)
    {}

    Token const& token() const noexcept { return m_token; }

    std::string message() const
    {
        std::ostringstream ss;
        ss << "Token error [" << m_token.info().line
           << ":" << m_token.info().column << "]"
           << ": " << what();
        return ss.str();
    }
    
private:
    Token m_token;
};

//! Common exception class to keep track of offending nodes.
class NodeError : public std::runtime_error
{
public:
    NodeError(Node* node, std::string const& what) :
        std::runtime_error(what),
        m_node(node)
    {}

    Node* node() const noexcept { return m_node; }
    
private:
    Node* m_node;
};

// --------------------------------------------------------------------------------------
// Lexer
// --------------------------------------------------------------------------------------

class Lexer
{
public:
    Lexer(std::istream& in) :
        m_in(in)
    { M_init(); }

    ~Lexer()
    {}
    
    //! Get the next token from the input stream.
    Token get()
    {
        Token tok = m_nextToken;
        m_nextToken = M_getToken();
        return tok;
    }

    //! Seek for the next token in the input stream
    //!   (but do NOT extract it).
    Token const& seek() const
    { return m_nextToken; }
    
private:
    //! Init the lexer (called from constructors).
    void M_init()
    {
        m_currentInfo.line = 1;
        m_currentInfo.column = 0;

        // Get first char (m_nextChar is now valid)
        m_nextChar = 0;
        M_getChar();
        // Get first token (m_nextToken is now valid)
        m_nextToken = M_getToken();
    }

    //! Extract a character from the input stream.
    //! This method manages the position in the input
    //!   stream for debug and error messages purposes.
    int M_getChar()
    {
        int ch = m_nextChar;
        m_nextChar = m_in.get();

        // Update stream information
        if (ch == '\n')
        {
            ++m_currentInfo.line;
            m_currentInfo.column = 0;
        }
        ++m_currentInfo.column;

        return ch;
    }

    //! Skip whitespaces (and new lines).
    void M_skipWs()
    {
        while (std::isspace(m_nextChar) || m_nextChar == '\n')
        {
            M_getChar();
            if (m_nextChar < 0) return;
        }
    }

    //! Skip comments, starting with a hashtag '#'.
    void M_skipComments()
    {
        while (m_nextChar == '#')
        {
            while (m_nextChar != '\n')
            {
                M_getChar();
                if (m_nextChar < 0) return;
            }

            M_skipWs();
        }
    }

    //! Skip whitespaces and comments.
    void M_skip()
    {
        M_skipWs();
        M_skipComments();
        M_skipWs();
    }

    //! Extract a token from the input stream.
    Token M_getToken()
    {
        // Skip whitespaces and comments
        M_skip();

        // Create token (bad by default), and save current stream information
        Token token = Token::Bad;
        Token::Info info = m_currentInfo;

        // Handle EOF gracefully
        if (m_nextChar < 0)
            token = Token::Eof;
        else
        {
            bool eatlast = true;

            if (m_nextChar == '{')
                token = Token::LeftBrace;
            else if (m_nextChar == '}')
                token = Token::RightBrace;
            else if (m_nextChar == '[')
                token = Token::LeftBracket;
            else if (m_nextChar == ']')
                token = Token::RightBracket;
            else if (m_nextChar == ',')
                token = Token::Comma;
            else if (m_nextChar == ':')
                token = Token::Colon;
            else if (m_nextChar == 't')
                token = M_matchKeyword(Token::True, "true");
            else if (m_nextChar == 'f')
                token = M_matchKeyword(Token::False, "false");
            else if (m_nextChar == 'n')
                token = M_matchKeyword(Token::Null, "null");
            else
            {
                // Includes
                if (m_nextChar == '@')
                {
                    M_getChar();

                    std::string path;

                    bool ok = m_nextChar == '"';
                    M_getChar();

                    while (ok && m_nextChar != '"')
                    {
                        // Stop if EOF is encountered
                        if (m_nextChar < 0)
                            ok = false;

                        path += M_getChar();
                    }

                    ok = ok && m_nextChar == '"';

                    if (!ok)
                        token = Token::Bad;
                    else
                        token = Token(Token::Include, path);
                }
                // String and identifiers
                else if (m_nextChar == '"')
                {
                    // Eat the double quotes
                    M_getChar();

                    std::string value;
                    bool ok = true;

                    while (ok && m_nextChar != '"')
                    {
                        // Stop if EOF is encountered
                        if (m_nextChar < 0)
                            ok = false;

                        // Handle some escape sequences
                        if (m_nextChar == '\\')
                        {
                            // Eat the backslash
                            M_getChar();

                            // Get the escaped character (and handle EOF)
                            int ch = M_getChar();
                            if (ch < 0)
                                ok = false;

                            if (ch == '\\')
                                value += '\\';
                            else if (ch == '"')
                                value += '"';
                            else if (ch == 'n')
                                value += '\n';
                            else if (ch == 't')
                                value += 't';
                            else
                                ok = false;
                        }
                        // Simple character
                        else
                            value += M_getChar();
                    }

                    // Strings must end with another double quotes
                    ok = ok && m_nextChar == '"';

                    if (!ok)
                        token = Token::Bad;
                    else
                        token = Token(Token::String, value);
                }
                // Numbers
                if (m_nextChar == '-' || m_nextChar == '.' || std::isdigit(m_nextChar))
                {
                    std::string value;
                    bool ok = true;

                    // Eventual sign
                    if (m_nextChar == '-')
                    {
                        value += M_getChar();
                        if (m_nextChar < 0)
                            ok = false;
                    }

                    // Eventual integer part
                    while (ok && std::isdigit(m_nextChar))
                    {
                        value += M_getChar();
                        if (m_nextChar < 0)
                            ok = false;
                    }

                    // Eventual floating part
                    if (ok && m_nextChar == '.')
                    {
                        // Eat the dot
                        value += M_getChar();
                        if (m_nextChar < 0)
                            ok = false;

                        // Don't allow empty floating parts
                        //   (as we already allow empty integer parts, we
                        //   would end up with '.' as a valid number...)
                        if (!std::isdigit(m_nextChar))
                            ok = false;

                        // Get the floating part
                        while (ok && std::isdigit(m_nextChar))
                        {
                            value += M_getChar();
                            if (m_nextChar < 0)
                                ok = false;
                        }
                    }

                    // Eventual exponent part
                    if (ok && (m_nextChar == 'e' || m_nextChar == 'E'))
                    {
                        // Eat the 'e'
                        value += M_getChar();
                        if (m_nextChar < 0)
                            ok = false;

                        // Eventual exponent's sign
                        if (m_nextChar == '-')
                        {
                            value += M_getChar();
                            if (m_nextChar < 0)
                                ok = false;
                        }

                        if (!std::isdigit(m_nextChar))
                            ok = false;

                        while (ok && std::isdigit(m_nextChar))
                        {
                            value += M_getChar();
                            if (m_nextChar < 0)
                                ok = false;
                        }
                    }

                    if (!ok)
                        token = Token::Bad;
                    else
                        token = Token(Token::Number, value);

                    eatlast = false;
                }
            }

            // Get the last char from previous rules
            if (eatlast && token.type() != Token::Bad)
                M_getChar();
        }

        // Set stream information for this token and return
        token.setInfo(info);
        return token;
    }

    //! Match a keyword in the input stream, returning a token
    //!   with the given type (or a Bad one in case of a mismatch).
    Token M_matchKeyword(Token::Type type, std::string const& kw)
    {
        for (unsigned int i = 0; i < kw.size(); ++i)
        {
            if (m_nextChar != kw[i])
                return Token::Bad;
            if (i == kw.size()-1)
                break;
            M_getChar();
        }

        return type;
    }
    
private:
    std::istream& m_in;
    
    int m_nextChar;
    Token m_nextToken;
    Token::Info m_currentInfo;
};

// --------------------------------------------------------------------------------------
// Nodes
// --------------------------------------------------------------------------------------

class NumberNode;
class BooleanNode;
class StringNode;
class ObjectNode;
class ArrayNode;

class Node
{
    //! Needed to access private members of *Node from
    //!   M_serialize() and M_multiline().
    friend class ObjectNode;
    friend class ArrayNode;
public:
    enum Type
    {
        Number,
        Boolean,
        String,
        Object,
        Array,
        Null
    };
    
protected:
    Node(Token const& token = Token()) : m_token(token) {}
    
public:
    virtual ~Node() {}
    
    //! Get the type of this node.
    virtual Type type() const = 0;

    //! Get the type string of this node.
    static std::string typeName(Type type)
    {
        if (type == Number) return "Number";
        else if (type == Boolean) return "Boolean";
        else if (type == String) return "String";
        else if (type == Object) return "Object";
        else if (type == Array) return "Array";
        else if (type == Null) return "Null";
        
        return "?";
    }

    //! Serialize the JSON tree whose root is this node to the
    //!   given output stream.
    //! If indent == false, no indentation is outputted
    //!   (and you get a compact, single-line output).
    void serialize(std::ostream& out, bool indent = true) const
    { M_serialize(out, 0, indent); }
    
    template <typename T>
    T* downcast()
    { return M_downcast((T*) 0); }

    Token const& token() const
    { return m_token; }
    
protected:
    virtual void M_serialize(std::ostream& out, int level, bool indent) const = 0;
    virtual bool M_multiline() const = 0;
    
    template <typename T>
    T* M_downcast(T*)
    { return 0; }
    
    NumberNode* M_downcast(NumberNode*)
    { return M_safeCast<Number, NumberNode>(); }
    
    BooleanNode* M_downcast(BooleanNode*)
    { return M_safeCast<Boolean, BooleanNode>(); }
    
    StringNode* M_downcast(StringNode*)
    { return M_safeCast<String, StringNode>(); }
    
    ObjectNode* M_downcast(ObjectNode*)
    { return M_safeCast<Object, ObjectNode>(); }
    
    ArrayNode* M_downcast(ArrayNode*)
    { return M_safeCast<Array, ArrayNode>(); }
    
    template <Type tp, typename T>
    T* M_safeCast()
    {
        if (type() != tp) return 0;
        return (T*) this;
    }

private:
    Token m_token;
};

class NumberNode : public Node
{
public:
    NumberNode(float value, Token const& token = Token()) : Node(token), m_value(value) {}
    
    Type type() const { return Number; }
    float value() const { return m_value; }
    
private:
    void M_serialize(std::ostream& out, int level, bool indent) const
    {
        std::string pre = "";
        for (int i = 0; indent && i < level; ++i) pre += " ";
        
        out << pre << m_value;
    }

    bool M_multiline() const { return false; }
    
private:
    float m_value;
};

class BooleanNode : public Node
{
public:
    BooleanNode(bool value, Token const& token = Token()) : Node(token), m_value(value) {}
    
    Type type() const { return Boolean; }
    bool value() const { return m_value; }
    
private:
    void M_serialize(std::ostream& out, int level, bool indent) const
    {
        std::string pre = "";
        for (int i = 0; indent && i < level; ++i) pre += " ";
        
        out << pre << (m_value ? "true" : "false");
    }

    bool M_multiline() const { return false; }
    
private:
    bool m_value;
};

class StringNode : public Node
{
public:
    StringNode(std::string const& value, Token const& token = Token()) : Node(token), m_value(value) {}
    
    Type type() const { return String; }
    std::string const& value() const { return m_value; }

    std::string escapedValue() const
    {
        std::string escaped;

        for (auto c : m_value)
        {
            if (c == '\n')
                escaped += "\\n";
            else if (c == '\t')
                escaped += "\\t";
            else if (c == '"')
                escaped += "\\\"";
            else
                escaped += c;
        }

        return escaped;
    }
    
private:
    void M_serialize(std::ostream& out, int level, bool indent) const
    {
        std::string pre = "";
        for (int i = 0; indent && i < level; ++i) pre += " ";
        
        out << pre << '"' << escapedValue() << '"';
    }

    bool M_multiline() const { return false; }
    
private:
    std::string m_value;
};

class ObjectNode : public Node
{
public:
    ObjectNode(Token const& token = Token()) : Node(token) {}

    ~ObjectNode()
    {
        for (std::map<std::string, Node*>::iterator it = m_impl.begin();
             it != m_impl.end(); ++it)
             delete it->second;
    }
    
    Type type() const { return Object; }
    bool exists(std::string const& key) { return m_impl.find(key) != m_impl.end(); }
    Node*& get(std::string const& key) { return m_impl[key]; }
    Node* get(std::string const& key) const { return m_impl.at(key); }
    std::map<std::string, Node*>& impl() { return m_impl; }
    std::map<std::string, Node*> const& impl() const { return m_impl; }
    
private:
    void M_serialize(std::ostream& out, int level, bool indent) const
    {
        std::string pre = "";
        for (int i = 0; indent && i < level; ++i) pre += " ";
        
        out << pre << '{';
        if (indent) out << std::endl;
        
        std::map<std::string, Node*>::const_iterator it;
        for (it = m_impl.begin(); it != m_impl.end(); ++it)
        {
            if (indent) out << pre << "    ";
            out << '"' << it->first << "\": ";
            
            if (indent && it->second->M_multiline())
            {
                out << std::endl;
                it->second->M_serialize(out, level + 4, indent);
            }
            else
            {
                it->second->M_serialize(out, 0, false);
            }
            
            // (++it)-- returns the next iterator value, leaving
            //   it unchanged.
            if ((++it)-- != m_impl.end())
                out << ", ";
            if (indent) out << std::endl;
        }
        
        out << pre << '}';
    }

    bool M_multiline() const { return true; }
    
private:
    std::map<std::string, Node*> m_impl;
};

class ArrayNode : public Node
{
public:
    ArrayNode(Token const& token = Token()) : Node(token) {}

    ~ArrayNode()
    {
        for (unsigned int i = 0; i < m_impl.size(); ++i)
            delete m_impl[i];
    }
    
    Type type() const { return Array; }
    size_t size() const { return m_impl.size(); }
    Node*& at(size_t i)
    {
        if (i >= m_impl.size()) throw std::domain_error("json::ArrayNode::at: index out of bounds");
        return m_impl[i];
    }

    Node* at(size_t i) const
    {
        if (i >= m_impl.size()) throw std::domain_error("json::ArrayNode::at: index out of bounds");
        return m_impl[i];
    }

    std::vector<Node*>& impl() { return m_impl; }
    std::vector<Node*> const& impl() const { return m_impl; }
    
private:
    void M_serialize(std::ostream& out, int level, bool indent) const
    {
        std::string pre = "";
        for (int i = 0; indent && i < level; ++i) pre += " ";
        
        out << pre << '[';
        bool multi = indent && M_multiline();
        if (multi) out << std::endl;
        
        for (unsigned int i = 0; i < m_impl.size(); ++i)
        {
            if (multi)
            {
                m_impl[i]->M_serialize(out, level + 4, indent);
            }
            else
            {
                m_impl[i]->M_serialize(out, 0, false);
            }
            
            if (i != m_impl.size()-1)
                out << ", ";
            if (multi) out << std::endl;
        }
        
        out << pre << ']';
    }

    bool M_multiline() const
    {
        for (unsigned int i = 0; i < m_impl.size(); ++i)
            if (m_impl[i]->M_multiline())
                return true;
        return false;
    }
    
private:
    std::vector<Node*> m_impl;
};

class NullNode : public Node
{
public:
    NullNode(Token const& token = Token()) : Node(token) {}
    
    Type type() const { return Null; }
    
private:
    void M_serialize(std::ostream& out, int level, bool indent) const
    {
        std::string pre = "";
        for (int i = 0; indent && i < level; ++i) pre += " ";

        out << pre << "null";
    }

    bool M_multiline() const { return false; }
    
private:
};

// --------------------------------------------------------------------------------------
// Parser
// --------------------------------------------------------------------------------------

class Parser
{
public:
    Parser(Lexer& lex) : m_lex(lex) {}
    ~Parser() {}
    
    Node* parse()
    {
        if (m_lex.seek().type() == Token::LeftBrace)
            return M_object();

        return M_array();
    }
    
private:
    Node* M_atom()
    {
        Token next = m_lex.seek();
        if (next.type() == Token::Bad)
        {
            throw TokenError(next, "bad token");
        }
        else if (next.type() == Token::True ||
                 next.type() == Token::False)
        {
            m_lex.get();
            return new BooleanNode(next.type() == Token::True, next);
        }
        else if (next.type() == Token::Null)
        {
            m_lex.get();
            return new NullNode(next);
        }
        else if (next.type() == Token::Number)
        {
            m_lex.get();
            
            float value;
            try {
                value = std::stof(next.value());
            } catch (std::exception const&) {
                value = 0.0f;
            }

            return new NumberNode(value, next);
        }
        else if (next.type() == Token::String)
        {
            m_lex.get();
            return new StringNode(next.value(), next);
        }
        else if (next.type() == Token::LeftBrace)
            return M_object();
        else if (next.type() == Token::LeftBracket)
            return M_array();
        else if (next.type() == Token::Include)
        {
            Node* tree = JSON_NAMESPACE(parse)(next.value());
            m_lex.get();
            return tree;
        }

        return 0;
    }

    Node* M_object()
    {
        // Eat the opening {
        if (m_lex.seek().type() != Token::LeftBrace)
            throw TokenError(m_lex.seek(), "expected `{' at beginning of object declaration");
        
        // Create appropriate node
        ObjectNode* node = new ObjectNode(m_lex.get());

        // Parse object entries
        for (;;)
        {
            // Allow empty objects
            if (m_lex.seek().type() == Token::RightBrace)
                break;

            // Get key identifier
            if (m_lex.seek().type() != Token::String)
                throw TokenError(m_lex.seek(), "expected a identifier key");
            Token token = m_lex.get();
            std::string key = token.value();

            if (node->exists(key))
                throw TokenError(token, "redifinition of object entry `" + key + "'");

            // Get the separator
            if (m_lex.seek().type() != Token::Colon)
                throw TokenError(m_lex.seek(), "expected `:' after identifier");
            m_lex.get();

            // Parse the object element value
            node->impl()[key] = M_atom();

            // Eat comma, if needed
            if (m_lex.seek().type() == Token::Comma)
                m_lex.get();
            else
                break;
        }

        // Eat the closing }
        if (m_lex.seek().type() != Token::RightBrace)
            throw TokenError(m_lex.seek(), "expected `}' at end of object declaration");
        m_lex.get();

        return node;
    }

    Node* M_array()
    {
        // Eat the opening [
        if (m_lex.seek().type() != Token::LeftBracket)
            throw TokenError(m_lex.seek(), "expected `[' at beginning of array definition");

        // Create appropriate node
        ArrayNode* node = new ArrayNode(m_lex.get());

        // Parse array entries
        for (;;)
        {
            // Allow empty arrays
            if (m_lex.seek().type() == Token::RightBracket)
                break;

            // Get array element
            node->impl().push_back(M_atom());

            // Get comma, if needed
            if (m_lex.seek().type() == Token::Comma)
                m_lex.get();
            else
                break;
        };

        if (m_lex.seek().type() != Token::RightBracket)
            throw TokenError(m_lex.seek(), "expected `]' at end of array declaration");
        m_lex.get();

        return node;
    }
    
private:
    Lexer& m_lex;
};

// --------------------------------------------------------------------------------------
// Template
// --------------------------------------------------------------------------------------

//! Common abstract template element interface.
class Element
{
public:
    enum Type
    {
        User,
        Scalar,
        POD,
        Raw,
        Vector,
        Map,
        Object,
        Array
    };
    
public:
    Element() : refs(1) {}
    virtual ~Element() {}
    virtual Type type() const = 0;
    virtual void extract(Node* node) const = 0;
    virtual Node* synthetize() const = 0;
    virtual bool isConst() const = 0;
    
public:
    int refs;
};

//! Terminal element interface (specialized below).
template <typename T>
class Terminal : public Element
{
public:
    Terminal(T&)
    {}

    Terminal(T const&)
    {}
    
    Type type() const
    { return Element::Scalar; }
    
    void extract(Node* node)
    { throw NodeError(node, "json::Terminal::extract: direct extraction is not supported for this type"); }
    
    Node* synthetize() const
    { return 0; }

    bool isConst() const
    { return false; }
};

//! Generic scalar element.
template <Node::Type tp, typename N, typename T>
class Scalar : public Element
{
public:
    Scalar(T& ref) :
        m_temp(nullptr),
        m_ref(ref),
        m_is_const(false)
    {}

    Scalar(T const& const_ref) :
        m_temp(nullptr),
        m_ref(const_cast<T&>(const_ref)),
        m_is_const(true)
    {}

    Scalar(T&& temp) :
        m_temp(std::make_unique<T>(temp)),
        m_ref(*m_temp),
        m_is_const(true)
    {}
    
    Type type() const
    { return Element::Scalar; }
    
    void extract(Node* node) const
    {
        if (m_is_const)
            throw NodeError(node, "json::Scalar[const]::extract extracting to const binding");

        if (node->type() != tp)
            throw NodeError(node, "json::Scalar::extract: expecting a node of type " + Node::typeName(tp));
        m_ref = node->downcast<N>()->value();
    }
    
    Node* synthetize() const
    { return new N(m_ref); }

    bool isConst() const
    { return false; }
    
protected:
    std::unique_ptr<T> m_temp;
    T& m_ref;
    bool m_is_const;
};

//! Generic POD element.
template <typename T>
class POD : public Element
{
public:
    POD(T& ref) :
        m_ref(ref),
        m_is_const(false)
    {}

    POD(T const& ref) :
        m_ref(const_cast<T&>(ref)),
        m_is_const(true)
    {}

    Type type() const
    { return Element::POD; }

    void extract(Node* node) const
    {
        if (m_is_const)
            throw NodeError(node, "json::POD[const]::extract: extracting to const binding");

        if (node->type() != Node::String)
            throw NodeError(node, "json::POD::extract: expecting a string node");

        std::string as_hex = node->downcast<json::StringNode>()->value();
        if (as_hex.size() % 2 != 0 || as_hex.size() / 2 != sizeof(T))
        {
            std::ostringstream ss;
            ss << "json::POD::extract: bad buffer size (expecting " << sizeof(T) << ", got ";
            ss << as_hex.size() / 2 << ")";
            throw NodeError(node, ss.str());
        }

        uint8_t* data = reinterpret_cast<uint8_t*>(&m_ref);
        for (std::size_t byte = 0; byte < as_hex.size()/2; ++byte)
        {
            std::istringstream ss(as_hex.substr(2*byte, 2));
            ss >> std::hex;
            int value;
            ss >> value;
            data[byte] = static_cast<uint8_t>(value & 0xFF);
        }
    }

    Node* synthetize() const
    {
        std::ostringstream ss;
        uint8_t* data = reinterpret_cast<uint8_t*>(&m_ref);

        for (std::size_t byte = 0; byte < sizeof(T); ++byte)
            ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(data[byte]);

        return new StringNode(ss.str());
    }

    bool isConst() const
    { return m_is_const; }

private:
    T& m_ref;
    bool m_is_const;
};

//! Used to tag types as POD
template <typename T>
struct tag_as_pod_impl
{
public:
    tag_as_pod_impl(T& ref) : ref(ref)
    {}

    T& ref;
};

//! Used to tag types as POD (const)
template <typename T>
struct tag_as_const_pod_impl
{
public:
    tag_as_const_pod_impl(T const& ref) : ref(ref)
    {}

    T const& ref;
};

//! Used to tag types as POD
template<typename T>
static tag_as_pod_impl<T> ref_as_pod(T& ref)
{ return tag_as_pod_impl<T>(ref); }

template<typename T>
static tag_as_const_pod_impl<T> ref_as_pod(T const& ref)
{ return tag_as_const_pod_impl<T>(ref); }


//! Generic raw element.
template <typename T>
class Raw : public Element
{
public:
    Raw(T*& ptr, std::size_t& size) :
        m_ptr(&ptr),
        m_size(&size),
        m_is_const(false)
    {}

    Raw(T const* ptr, std::size_t size) :
        m_ptr(new T*(const_cast<T*>(ptr))),
        m_size(new std::size_t(size)),
        m_is_const(true)
    {}

    ~Raw()
    {
        if (m_is_const)
        {
            delete m_ptr;
            delete m_size;
        }
    }

    Type type() const
    { return Element::Raw; }

    void extract(Node* node) const
    {
        if (m_is_const)
            throw NodeError(node, "json::Raw[const]::extract: extracting to const binding");

        if (*m_ptr != 0)
            throw NodeError(node, "json::Raw::extract: target memory is already allocated");

        if (node->type() == Node::Null)
        {
            *m_size = 0UL;
            *m_ptr = nullptr;
            return;
        }

        if (node->type() != Node::String)
            throw NodeError(node, "json::Raw::extract: expecting a string node");

        std::string as_hex = node->downcast<json::StringNode>()->value();
        if (as_hex.size() % 2 != 0)
            throw NodeError(node, "json::Raw::extract: bad buffer size");

        *m_size = as_hex.size() / (2 * sizeof(T));
        *m_ptr = new T[*m_size];

        for (std::size_t i = 0; i < *m_size; ++i)
        {
            uint8_t* data = reinterpret_cast<uint8_t*>(&(*m_ptr)[i]);

            for (std::size_t byte = 0; byte < sizeof(T); ++byte)
            {
                std::string nibbles = as_hex.substr(2*i * sizeof(T) + 2*byte, 2);

                std::istringstream ss(nibbles);
                ss >> std::hex;
                int value;
                ss >> value;

                data[byte] = static_cast<uint8_t>(value & 0xFF);
            }
        }
    }

    Node* synthetize() const
    {
        if (!*m_size || !*m_ptr)
            return new NullNode();

        std::ostringstream ss;

        for (std::size_t i = 0; i < *m_size; ++i)
        {
            uint8_t* data = reinterpret_cast<uint8_t*>(&(*m_ptr)[i]);
            for (std::size_t byte = 0; byte < sizeof(T); ++byte)
                ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(data[byte]);
        }

        return new StringNode(ss.str());
    }

    bool isConst() const
    { return m_is_const; }

private:
    T** m_ptr;
    std::size_t* m_size;
    bool m_is_const;
};

//! Used to tag types as RAW
template <typename T>
struct tag_as_raw_impl
{
public:
    tag_as_raw_impl(T*& ptr, std::size_t& size) : ptr(ptr), size(size)
    {}

    T*& ptr;
    std::size_t& size;
};

//! Used to tag types as RAW (const)
template <typename T>
struct tag_as_const_raw_impl
{
public:
    tag_as_const_raw_impl(T const* ptr, std::size_t size) : ptr(ptr), size(size)
    {}

    T const* ptr;
    std::size_t size;
};

//! Used to tag types as RAW
template<typename T>
static tag_as_raw_impl<T> ref_as_raw(T*& ptr, std::size_t& size)
{ return tag_as_raw_impl<T>(ptr, size); }

template<typename T>
static tag_as_const_raw_impl<T> ref_as_raw(T const* ptr, std::size_t size)
{ return tag_as_const_raw_impl<T>(ptr, size); }

//! Generic vector element.
template <typename T>
class Vector : public Element
{
public:
    Vector(std::vector<T>& ref) :
        m_temp(nullptr),
        m_ref(ref),
        m_is_const(false)
    {}

    Vector(std::vector<T> const& const_ref) :
        m_temp(nullptr),
        m_ref(const_cast<std::vector<T>&>(const_ref)),
        m_is_const(true)
    {}

    Vector(std::vector<T>&& temp) :
        m_temp(std::make_unique<std::vector<T>>(temp)),
        m_ref(*m_temp),
        m_is_const(true)
    {}
    
    Type type() const
    { return Element::Vector; }
    
    void extract(Node* node) const
    {
        if (m_is_const)
            throw NodeError(node, "json::Vector[const]::extract: extracting to const binding");

        if (node->type() != Node::Array)
            throw NodeError(node, "json::Vector::extract: expecting an array node");
        
        ArrayNode* arr = node->downcast<ArrayNode>();
        
        m_ref.clear();
        m_ref.reserve(arr->size());
        for (unsigned int i = 0; i < arr->size(); ++i)
        {
            T value;
            Terminal<T> term(value);
            term.extract(arr->at(i));
            m_ref.push_back(value);
        }
    }
    
    Node* synthetize() const
    {
        ArrayNode* arr = new ArrayNode();
        for (unsigned int i = 0; i < m_ref.size(); ++i)
        {
            Terminal<T> term(m_ref[i]);
            arr->impl().push_back(term.synthetize());
        }
        return arr;
    }

    bool isConst() const
    { return m_is_const; }
    
private:
    std::unique_ptr<std::vector<T>> m_temp;
    std::vector<T>& m_ref;
    bool m_is_const;
};

//! Generic map element.
template <typename T>
class Map : public Element
{
public:
    Map(std::map<std::string, T>& ref) :
        m_temp(nullptr),
        m_ref(ref),
        m_is_const(false)
    {}

    Map(std::map<std::string, T> const& ref) :
        m_temp(nullptr),
        m_ref(const_cast<std::map<std::string, T>&>(ref)),
        m_is_const(true)
    {}

    Map(std::map<std::string, T>&& temp) :
        m_temp(std::make_unique<T>(temp)),
        m_ref(*m_temp),
        m_is_const(true)
    {}
    
    Type type() const
    { return Element::Map; }
    
    void extract(Node* node) const
    {
        if (m_is_const)
            throw NodeError(node, "json::Map[const]::extract: extracting to const binding");

        if (node->type() != Node::Object)
            throw NodeError(node, "json::Map::extract: expecting an object node");
        
        ObjectNode* obj = node->downcast<ObjectNode>();
        
        m_ref.clear();
        for (std::map<std::string, Node*>::iterator it = obj->impl().begin();
            it != obj->impl().end(); ++it)
        {
            T value;
            Terminal<T> term(value);
            term.extract(it->second);
            m_ref[it->first] = value;
        }
    }
    
    Node* synthetize() const
    {
        ObjectNode* obj = new ObjectNode();
        for (typename std::map<std::string, T>::iterator it = m_ref.begin();
            it != m_ref.end(); ++it)
        {
            T value;
            Terminal<T> term(it->second);
            obj->impl()[it->first] = term.synthetize();
        }
        return obj;
    }

    bool isConst() const
    { return m_is_const; }
    
private:
    std::unique_ptr<std::map<std::string, T>> m_temp;
    std::map<std::string, T>& m_ref;
    bool m_is_const;
};

//! Below are the specializations for the Terminal
//!   element class.
//! Instances that are not specialized here will systematically trigger errors
//!   (see beginning of this file).

template <>
class Terminal<int32_t> : public Scalar<Node::Number, NumberNode, int32_t>
{
public:
    using Scalar::Scalar;
};

template <>
class Terminal<uint32_t> : public Scalar<Node::Number, NumberNode, uint32_t>
{
public:
    using Scalar::Scalar;
};

template <>
class Terminal<int64_t> : public Scalar<Node::Number, NumberNode, int64_t>
{
public:
    using Scalar::Scalar;
};

template <>
class Terminal<uint64_t> : public Scalar<Node::Number, NumberNode, uint64_t>
{
public:
    using Scalar::Scalar;
};

template <>
class Terminal<float> : public Scalar<Node::Number, NumberNode, float>
{
public:
    using Scalar::Scalar;
};

template <>
class Terminal<double> : public Scalar<Node::Number, NumberNode, double>
{
public:
    using Scalar::Scalar;
};

template <>
class Terminal<bool> : public Scalar<Node::Boolean, BooleanNode, bool>
{
public:
    using Scalar::Scalar;
};

template <>
class Terminal<std::string> : public Scalar<Node::String, StringNode, std::string>
{
public:
    using Scalar::Scalar;
};

template <>
class Terminal<const char*> : public Scalar<Node::String, StringNode, std::string>
{
public:
    Terminal(const char* ptr) :
        Scalar(static_cast<std::string const&>(m_str)),
        m_str(ptr)
    {}

private:
    std::string m_str;
};

template <typename T>
class Terminal<std::vector<T> > : public Vector<T>
{
public:
    using Vector<T>::Vector;
};

template <typename T>
class Terminal<std::map<std::string, T> > : public Map<T>
{
public:
    using Map<T>::Map;
};

template <typename T>
class Terminal<tag_as_pod_impl<T> > : public POD<T>
{
public:
    Terminal(tag_as_pod_impl<T> ref) : POD<T>(ref.ref)
    {}
};

template <typename T>
class Terminal<tag_as_const_pod_impl<T> > : public POD<T>
{
public:
    Terminal(tag_as_const_pod_impl<T> ref) : POD<T>(ref.ref)
    {}
};

template <typename T>
class Terminal<tag_as_raw_impl<T> > : public Raw<T>
{
public:
    Terminal(tag_as_raw_impl<T> ref) : Raw<T>(ref.ptr, ref.size)
    {}
};

template <typename T>
class Terminal<tag_as_const_raw_impl<T> > : public Raw<T>
{
public:
    Terminal(tag_as_const_raw_impl<T> ref) : Raw<T>(ref.ptr, ref.size)
    {}
};

//! An object element class.
class Object : public Element
{
public:
    Object() {}
    ~Object()
    {
        for (std::map<std::string, Element*>::const_iterator it = m_elements.begin();
             it != m_elements.end(); ++it)
        {
            if (!--it->second->refs)
                delete it->second;
        }
    }
    
    void bind(std::string const& name, Element* elem)
    {
        if (m_elements.find(name) != m_elements.end())
            throw std::runtime_error("json::Object::bind: element `" + name + "' is already bound");
        
        ++elem->refs;
        m_elements[name] = elem;
    }

    Type type() const { return Element::Object; }

    void extract(Node* node) const
    {
        if (node->type() != Node::Object)
            throw NodeError(node, "json::Object::extract: type mismatch");
        ObjectNode* obj = node->downcast<ObjectNode>();
        
        for (std::map<std::string, Element*>::const_iterator it = m_elements.begin();
             it != m_elements.end(); ++it)
        {
            if (!obj->exists(it->first))
                throw NodeError(node, "json::Object::extract: missing element `" + it->first + "'");
            
            it->second->extract(obj->get(it->first));
        }
    }

    Node* synthetize() const
    {
        ObjectNode* obj = new ObjectNode();
        for (std::map<std::string, Element*>::const_iterator it = m_elements.begin();
             it != m_elements.end(); ++it)
        {
            obj->impl()[it->first] = it->second->synthetize();
        }
        return obj;
    }

    bool isConst() const { return false; }
    
private:
    std::map<std::string, Element*> m_elements;
};

//! An array element class.
class Array : public Element
{
public:
    Array() {}
    ~Array()
    {
        for (unsigned int i = 0; i < m_elements.size(); ++i)
        {
            if (!--m_elements[i]->refs)
                delete m_elements[i];
        }
    }
    
    void bind(Element* elem)
    {
        ++elem->refs;
        m_elements.push_back(elem);
    }

    Type type() const { return Element::Array; }

    void extract(Node* node) const
    {
        if (node->type() != Node::Array)
            throw NodeError(node, "json::Array::extract: type mismatch");
        ArrayNode* arr = node->downcast<ArrayNode>();
        
        for (unsigned int i = 0; i < m_elements.size(); ++i)
        {
            if (i >= arr->size())
                throw NodeError(node, "json::Array::extract: size mismatch in array");
            
            m_elements[i]->extract(arr->at(i));
        }
    }

    Node* synthetize() const
    {
        ArrayNode* arr = new ArrayNode();
        for (unsigned int i = 0; i < m_elements.size(); ++i)
        {
            arr->impl().push_back(m_elements[i]->synthetize());
        }
        return arr;
    }

    bool isConst() const { return false; }
    
private:
    std::vector<Element*> m_elements;
};

//! A small helper Element overloaded class
//!   (that just defines type() const).
class UserElement : public Element
{
public:
    Type type() const
    { return Element::User; }
};

//! The final JSON template class.
class Template
{
public:
    Template() : m_impl(nullptr) {}
    Template(Template& cpy) : m_impl(nullptr) { operator=(cpy); }
    Template(Template const& cpy) : m_impl(nullptr) { operator=(cpy); }
    
    ~Template()
    { reset(); }
    
    Template& operator=(Template const& cpy)
    {
        m_impl = cpy.m_impl;
        if (m_impl)
            m_impl->refs++;
        return *this;
    }
    
    template <typename T>
    Template(T& ref) :
        m_impl(nullptr)
    { bind(ref); }

    template <typename T>
    Template(T const& const_ref) :
        m_impl(nullptr)
    { bind(const_ref); }

    template <typename T>
    Template(T&& temp) :
        m_impl(nullptr)
    { bind(std::forward<T>(temp)); }
    
    template <typename T>
    Template& bind(T& ref)
    {
        if (m_impl) throw std::runtime_error("json::Template::bind: template is already bound");
        m_impl = new Terminal<T>(ref);
        return *this;
    }
    
    template <typename T>
    Template& bind(T const& const_ref)
    {
        if (m_impl) throw std::runtime_error("json::Template::bind: template is already bound");
        m_impl = new Terminal<T>(const_ref);
        return *this;
    }
    
    template <typename T>
    Template& bind(T&& temp)
    {
        if (m_impl) throw std::runtime_error("json::Template::bind: template is already bound");
        m_impl = new Terminal<T>(std::forward<T>(temp));
        return *this;
    }
    
    Template& bind(std::string const& name, Template const& tpl)
    {
        if (m_impl && m_impl->type() != Element::Object)
            throw std::runtime_error("json::Template::bind: template is already bound");
        
        if (!m_impl)
            m_impl = new Object();
        
        ((Object*) m_impl)->bind(name, tpl.m_impl);
        return *this;
    }

    Template& bind_array(Template const& tpl)
    {
        if (m_impl && m_impl->type() != Element::Array)
            throw std::runtime_error("json::Template::bind_array: template is already bound");
        
        if (!m_impl)
            m_impl = new Array();
        
        ((Array*) m_impl)->bind(tpl.m_impl);
        return *this;
    }

    bool bound() const { return m_impl; }
    operator bool() const { return bound(); }
    
    void extract(Node* node) const
    {
        if (!m_impl)
            throw NodeError(node, "json::Template::extract: template is not bound !");
        
        m_impl->extract(node);
    }

    Node* synthetize() const
    {
        if (!m_impl)
            throw std::runtime_error("json::Template::synthetize: template is not bound !");
        
        return m_impl->synthetize();
    }

    void reset()
    {
        if (m_impl && !--m_impl->refs)
            delete m_impl;
        m_impl = nullptr;
    }
    
private:
    Element* m_impl;
};

//!
//! Below are the hacks to handle the std::vector<bool> specialization,
//!   for which the operator[] does not return a bool& but a special
//!   proxy class
//!

//! Generic scalar element.
//! We must use the hack of allocating a copy of the vector::reference,
//!   because if we store by value we violate the constness of extract,
//!   and if we store it by reference there is an "always true" bug
template <Node::Type tp, typename N>
class Scalar<tp, N, std::vector<bool>::reference> : public Element
{
public:
    Scalar(std::vector<bool>::reference& ref) :
        m_ref(new std::vector<bool>::reference(ref)),
        m_is_const(false)
    {}

    ~Scalar()
    { delete m_ref; }
    
    Type type() const
    { return Element::Scalar; }
    
    void extract(Node* node) const
    {
        if (m_is_const)
            throw NodeError(node, "json::Scalar[const]::extract: extracting to const binding");

        if (node->type() != tp)
            throw NodeError(node, "json::Scalar::extract: expecting a node of type " + Node::typeName(tp));
        *m_ref = node->downcast<N>()->value();
    }
    
    Node* synthetize() const
    { return new N(*m_ref); }

    bool isConst() const
    { return m_is_const; }
    
protected:
    std::vector<bool>::reference* m_ref;
    bool m_is_const;
};

template <>
class Terminal<std::vector<bool>::reference> : public Scalar<Node::Boolean, BooleanNode, std::vector<bool>::reference>
{
public:
    Terminal(std::vector<bool>::reference ref) : Scalar(ref)
    {}
};

//! Generic vector element.
template <>
class Vector<bool> : public Element
{
public:
    Vector(std::vector<bool>& ref) :
        m_ref(ref),
        m_is_const(false)
    {}

    Vector(std::vector<bool> const& ref) :
        m_ref(const_cast<std::vector<bool>&>(ref)),
        m_is_const(true)
    {}
    
    Type type() const
    { return Element::Vector; }
    
    void extract(Node* node) const
    {
        if (m_is_const)
            throw NodeError(node, "json::Vector[const]::extract: extracting to const binding");

        if (node->type() != Node::Array)
            throw NodeError(node, "json::Vector::extract: expecting an array node");
        
        ArrayNode* arr = node->downcast<ArrayNode>();
        
        m_ref.clear();
        m_ref.reserve(arr->size());
        for (unsigned int i = 0; i < arr->size(); ++i)
        {
            bool value;
            Terminal<bool> term(value);
            term.extract(arr->at(i));
            m_ref.push_back(value);
        }
    }
    
    Node* synthetize() const
    {
        ArrayNode* arr = new ArrayNode();
        for (unsigned int i = 0; i < m_ref.size(); ++i)
        {
            Terminal<std::vector<bool>::reference> term(m_ref[i]);
            arr->impl().push_back(term.synthetize());
        }
        return arr;
    }

    bool isConst() const
    { return m_is_const; }
    
private:
    std::vector<bool>& m_ref;
    bool m_is_const;
};

// --------------------------------------------------------------------------------------
// Utility functions
// --------------------------------------------------------------------------------------

Node* parse(std::string const& file)
{
    std::ifstream fs(file, std::ios::in);
    if (!fs)
        throw std::runtime_error("json::parse: unable to open \"" + file + "\"");
    return parse(fs);
}

Node* parse(std::istream& file)
{
    Lexer lexer(file);
    Parser parser(lexer);
    return parser.parse();
}

void serialize(Node* node, std::string const& file, bool indent)
{
    std::ofstream fs(file, std::ios::out);
    if (!fs)
        throw std::runtime_error("json::serialize: unable to open\"" + file + "\"");
    serialize(node, fs, indent);
}

void serialize(Node* node, std::ostream& file, bool indent)
{
    node->serialize(file, indent);
}

void extract(Template const& tpl, std::string const& file)
{
    Node* node = parse(file);
    tpl.extract(node);
    delete node;
}

void extract(Template const& tpl, std::istream& file)
{
    Node* node = parse(file);
    tpl.extract(node);
    delete node;
}

void synthetize(Template const& tpl, std::string const& file, bool indent)
{
    Node* node = tpl.synthetize();
    serialize(node, file, indent);
    delete node;
}

void synthetize(Template const& tpl, std::ostream& file, bool indent)
{
    Node* node = tpl.synthetize();
    serialize(node, file, indent);
    delete node;
}

JSON_END_NAMESPACE

#undef JSON_NAMESPACE
#undef JSON_END_NAMESPACE
#undef JSON_START_NAMESPACE

#endif // __JSON_H__
