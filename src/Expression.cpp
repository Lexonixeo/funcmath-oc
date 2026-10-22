// SPDX-License-Identifier: MIT
// Expression.cpp
// --------------
// Лексер и рекурсивный разбор инфиксных выражений.
//
// Задача слоя — не вычислять значения, а переписать выражение в префиксные
// слова. Благодаря этому правила типов, потоки, случайность и решатель
// остаются в существующих Level/Function и не дублируются здесь.
//
// Грамматика (упрощённо):
//
//   add    := mul (("+" | "-") mul)*
//   mul    := power (("*" | "/" | "%") power)*
//   power  := unary ("**" power)?          // правая ассоциативность
//   unary  := ("-" | "+")? primary
//   primary:= число | имя | "(" add ")" | имя "(" args ")"
//   args   := add ("," add)*
//
// Унарный минус разрешён только перед числовым литералом. Отрицание
// произвольного выражения пока не поддерживается: в уровне это создавало бы
// число 0 «из воздуха» и ломало расход чисел.

#include "Expression.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "Helper.h"

namespace {

enum class TokenKind {
    Atom,
    Operator,
    LeftParen,
    RightParen,
    Comma,
    End,
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    std::size_t pos = 0;
    bool spaceBefore = false;
    bool spaceAfter = false;
};

bool isSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool isOperatorChar(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%';
}

bool isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool isBareNumber(const std::string& text) {
    // Типизированные литералы (natural=2) под унарный минус не подходят:
    // для них есть форма integer=-3.
    if (text.empty() || text.find('=') != std::string::npos) return false;
    std::size_t i = 0;
    if (text[i] == '+' || text[i] == '-') ++i;
    if (i == text.size()) return false;
    for (; i < text.size(); ++i)
        if (!isDigit(text[i])) return false;
    return true;
}

std::string tokenDescription(const Token& token) {
    if (token.kind == TokenKind::End) return "конец выражения";
    if (token.kind == TokenKind::LeftParen) return "'('";
    if (token.kind == TokenKind::RightParen) return "')'";
    if (token.kind == TokenKind::Comma) return "','";
    return "'" + token.text + "'";
}

std::string atToken(const Token& token) {
    return "у позиции " + std::to_string(token.pos + 1);
}

bool tokenize(const std::string& input, std::vector<Token>& tokens,
              std::string& error) {
    tokens.clear();
    std::size_t i = 0;
    while (i < input.size()) {
        if (isSpace(input[i])) {
            ++i;
            continue;
        }

        const std::size_t start = i;
        Token token;
        token.pos = start;
        token.spaceBefore = start > 0 && isSpace(input[start - 1]);

        if (input[i] == '(') {
            token.kind = TokenKind::LeftParen;
            token.text = "(";
            ++i;
        } else if (input[i] == ')') {
            token.kind = TokenKind::RightParen;
            token.text = ")";
            ++i;
        } else if (input[i] == ',') {
            token.kind = TokenKind::Comma;
            token.text = ",";
            ++i;
        } else if (isOperatorChar(input[i])) {
            token.kind = TokenKind::Operator;
            if (input[i] == '*' && i + 1 < input.size() && input[i + 1] == '*') {
                token.text = "**";
                i += 2;
            } else {
                token.text = std::string(1, input[i]);
                ++i;
            }
        } else if (input[i] == '^') {
            error = "оператор '^' не поддерживается; используйте pow или **";
            return false;
        } else {
            token.kind = TokenKind::Atom;
            // Разделителями атома являются пробел, скобки, запятая и
            // арифметические операторы. Дефис внутри typed-литерала
            // (integer=-3) — часть атома, а не оператор.
            while (i < input.size()) {
                const char c = input[i];
                if (isSpace(c) || c == '(' || c == ')' || c == ',' ||
                    isOperatorChar(c)) {
                    // Знак после '=' — часть typed-литерала: integer=-3.
                    if ((c == '-' || c == '+') && i > start &&
                        input[i - 1] == '=') {
                        ++i;
                        continue;
                    }
                    break;
                }
                ++i;
            }
            if (i == start) {
                error = "неизвестный символ " + atToken(token);
                return false;
            }
            token.text = input.substr(start, i - start);
        }

        token.spaceAfter = i >= input.size() || isSpace(input[i]);
        tokens.push_back(std::move(token));
    }

    Token end;
    end.kind = TokenKind::End;
    end.pos = input.size();
    end.spaceBefore = !input.empty() && isSpace(input.back());
    end.spaceAfter = true;
    tokens.push_back(std::move(end));
    return true;
}

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

    bool parse(std::vector<std::string>& out, std::string& error) {
        out.clear();
        if (!parseAdd(out, error)) return false;
        if (peek().kind != TokenKind::End) {
            error = "лишний элемент " + tokenDescription(peek()) + " " +
                    atToken(peek());
            return false;
        }
        return true;
    }

private:
    const std::vector<Token>& tokens_;
    std::size_t pos_ = 0;

    const Token& peek() const { return tokens_[pos_]; }

    bool parseAdd(std::vector<std::string>& out, std::string& error) {
        std::vector<std::string> left;
        if (!parseMul(left, error)) return false;

        while (peek().kind == TokenKind::Operator &&
               (peek().text == "+" || peek().text == "-")) {
            const std::string op = peek().text;
            ++pos_;
            std::vector<std::string> right;
            if (!parseMul(right, error)) return false;
            out.clear();
            out.push_back(op == "+" ? "sum" : "sub");
            append(out, left);
            append(out, right);
            left = out;
        }
        out = std::move(left);
        return true;
    }

    bool parseMul(std::vector<std::string>& out, std::string& error) {
        std::vector<std::string> left;
        if (!parsePower(left, error)) return false;

        while (peek().kind == TokenKind::Operator &&
               (peek().text == "*" || peek().text == "/" || peek().text == "%")) {
            const std::string op = peek().text;
            ++pos_;
            std::vector<std::string> right;
            if (!parsePower(right, error)) return false;
            out.clear();
            out.push_back(op == "*" ? "mul" : (op == "/" ? "div" : "mod"));
            append(out, left);
            append(out, right);
            left = out;
        }
        out = std::move(left);
        return true;
    }

    bool parsePower(std::vector<std::string>& out, std::string& error) {
        std::vector<std::string> left;
        if (!parseUnary(left, error)) return false;

        if (peek().kind == TokenKind::Operator && peek().text == "**") {
            ++pos_;
            std::vector<std::string> right;
            // Правая ассоциативность: 2 ** 3 ** 2 = 2 ** (3 ** 2).
            if (!parsePower(right, error)) return false;
            out.clear();
            out.push_back("pow");
            append(out, left);
            append(out, right);
            return true;
        }
        out = std::move(left);
        return true;
    }

    bool parseUnary(std::vector<std::string>& out, std::string& error) {
        if (peek().kind == TokenKind::Operator && peek().text == "+") {
            ++pos_;
            return parseUnary(out, error);
        }
        if (peek().kind == TokenKind::Operator && peek().text == "-") {
            const Token minus = peek();
            ++pos_;
            if (peek().kind != TokenKind::Atom) {
                error = "унарный минус допустим только перед числом " +
                        atToken(minus);
                return false;
            }
            const Token& atom = peek();
            if (!isBareNumber(atom.text)) {
                if (atom.text.find('=') != std::string::npos) {
                    error = "унарный минус пишите внутри литерала: integer=-" +
                            atom.text.substr(atom.text.find('=') + 1);
                } else {
                    error = "унарный минус допустим только перед числом " +
                            atToken(minus);
                }
                return false;
            }
            out.clear();
            out.push_back("-" + atom.text);
            ++pos_;
            return true;
        }
        return parsePrimary(out, error);
    }

    bool parsePrimary(std::vector<std::string>& out, std::string& error) {
        if (peek().kind == TokenKind::LeftParen) {
            ++pos_;
            if (!parseAdd(out, error)) return false;
            if (peek().kind != TokenKind::RightParen) {
                error = "ожидается ')' " + atToken(peek());
                return false;
            }
            ++pos_;
            return true;
        }

        if (peek().kind != TokenKind::Atom) {
            error = "ожидается число, имя или '(' " + atToken(peek());
            return false;
        }

        const std::string name = peek().text;
        ++pos_;

        // Вызов функции: имя '(' аргументы ')'. Арность здесь не нужна:
        // запятые задают границы аргументов, а проверка арности остаётся
        // в существующем вычислителе.
        if (peek().kind == TokenKind::LeftParen) {
            ++pos_;
            std::vector<std::string> args;
            if (peek().kind == TokenKind::RightParen) {
                error = "у функции " + name + " нет аргументов " + atToken(peek());
                return false;
            }
            while (true) {
                std::vector<std::string> arg;
                if (!parseAdd(arg, error)) return false;
                append(args, arg);
                if (peek().kind == TokenKind::Comma) {
                    ++pos_;
                    if (peek().kind == TokenKind::RightParen) {
                        error = "лишняя запятая " + atToken(peek());
                        return false;
                    }
                    continue;
                }
                if (peek().kind == TokenKind::RightParen) {
                    ++pos_;
                    break;
                }
                error = "ожидается ',' или ')' " + atToken(peek());
                return false;
            }
            out.clear();
            out.push_back(name);
            append(out, args);
            return true;
        }

        out.clear();
        out.push_back(name);
        return true;
    }

    static void append(std::vector<std::string>& target,
                       const std::vector<std::string>& source) {
        target.insert(target.end(), source.begin(), source.end());
    }
};

}  // namespace

namespace expression {

bool hasInfixOperator(const std::string& text) {
    std::vector<Token> tokens;
    std::string error;
    if (!tokenize(text, tokens, error)) return true;

    int depth = 0;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens[i];
        if (token.kind == TokenKind::LeftParen) {
            ++depth;
            continue;
        }
        if (token.kind == TokenKind::RightParen) {
            if (depth > 0) --depth;
            continue;
        }
        if (token.kind != TokenKind::Operator) continue;
        if (token.text == "+" || token.text == "*" || token.text == "/" ||
            token.text == "%" || token.text == "**")
            return true;
        if (token.text != "-") continue;
        if (i == 0) continue;

        const Token& previous = tokens[i - 1];
        if (previous.kind == TokenKind::LeftParen ||
            previous.kind == TokenKind::Comma ||
            previous.kind == TokenKind::Operator)
            continue;

        // "sum 1 -2" — старый префикс с отрицательным литералом. Внутри
        // скобок ("f(1 -2, 3)") тот же минус уже считается двоичным.
        if (depth == 0 && previous.kind == TokenKind::Atom &&
            token.spaceBefore && !token.spaceAfter)
            continue;
        return true;
    }
    return false;
}

bool hasCallSyntax(const std::string& text) {
    std::vector<Token> tokens;
    std::string error;
    if (!tokenize(text, tokens, error)) return true;
    if (tokens.size() < 2) return false;
    return tokens[0].kind == TokenKind::Atom &&
           tokens[1].kind == TokenKind::LeftParen && !tokens[1].spaceBefore;
}

bool normalize(const std::string& text, std::vector<std::string>& prefix,
               std::string& error) {
    prefix.clear();
    std::vector<Token> tokens;
    if (!tokenize(text, tokens, error)) return false;
    Parser parser(tokens);
    return parser.parse(prefix, error);
}

}  // namespace expression
