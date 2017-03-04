#include "gen.h"

#include <cstdio>
#include <string>
#include <map>

namespace {

struct List {
    unsigned int len;
    unsigned int position;
};

struct Function {
    unsigned char reserved;
    unsigned char arg_count;
    unsigned short arg_stack_pointer;
    unsigned int position;
};

#pragma pack(1)

struct InstIntImm {
    unsigned char op;
    unsigned char reg;
    unsigned int int_imm;
};

struct InstFloatImm {
    unsigned char op;
    unsigned char reg;
    double float_imm;
};

struct InstDoubleInt {
    unsigned char op;
    unsigned char reg;
    unsigned int int_imm1;
    unsigned int int_imm2;
};

struct InstIntOnly {
    unsigned char op;
    unsigned int int_imm;
};

struct InstReg {
    unsigned char op;
    unsigned char reg;
};

struct InstVoid {
    unsigned char op;
};

#pragma pack()

enum OpType {
    kIntImm, kFloatImm,
    kReg, kRegReg, kRegInt, kVoid,
    kCall, kST, kINT, kMOVL
};

int op_type[] = {
    kVoid,
    kIntImm, kIntImm, kIntImm, kReg, kIntImm, kIntImm,
    kIntImm, kFloatImm, kIntImm, kFloatImm, kIntImm, kFloatImm, kIntImm, kFloatImm, kReg, kReg, kIntImm, kFloatImm,
    kIntImm, kFloatImm, kIntImm, kFloatImm, kIntImm, kFloatImm, kIntImm, kFloatImm, kIntImm, kIntImm,
    kRegInt, kIntImm, kIntImm, kCall, kVoid,
    kIntImm, kReg, kRegInt, kIntImm, kST, kIntImm, kINT,
    kReg, kReg, kReg, kReg, kReg, kReg,
    kRegReg, kRegReg, kRegReg,
    kRegReg, kMOVL, kRegReg, kRegReg
};

std::map<std::string, unsigned int> lab_list, lab_fill;
std::string section_tag;
unsigned int arg_stack_size = 0;

template <typename T>
inline void WriteBytes(std::ofstream &out, T &content) {
    out.write((char *)&content, sizeof(content));
}

}

void Generator::PrintError(const char *description) {
    std::fprintf(stderr, "\033[1mgenerator\033[0m(line %u): \033[31m\033[1merror:\033[0m %s\n", lexer_.line_pos(), description);
    ++error_num_;
}

void Generator::HandleLabelRef() {
    for (const auto &i : lab_list) {
        if (i.first == lexer_.lab_val()) {
            WriteBytes(out_, i.second);
            return;
        }
    }
    lab_fill.insert(std::map<std::string, unsigned int>::value_type(lexer_.lab_val(), out_.tellp()));
    unsigned int zero = 0;
    WriteBytes(out_, zero);
}

bool Generator::HandleOperator() {
    auto index = lexer_.op_val();
    int tok_type;

    auto Next = [&]() { return (tok_type = lexer_.NextToken()); };
    auto GenRegReg = [&, index](unsigned char rx, unsigned char ry) {
        InstReg inst = {index, (unsigned char)((rx << 4) + ry)};
        WriteBytes(out_, inst);
    };

    switch (op_type[index]) {
        case kIntImm: {
            if (Next() == kRegister) {
                auto reg = lexer_.reg_val();
                if (Next() == ',') {
                    Next();
                    if (tok_type == kRegister) {
                        GenRegReg(reg, lexer_.reg_val());
                        return true;
                    }
                    else if (tok_type == kNumber) {
                        InstIntImm inst = {index, (unsigned char)(reg << 4), lexer_.num_val()};
                        WriteBytes(out_, inst);
                        return true;
                    }
                    else if (tok_type == kLabelRef) {
                        GenRegReg(reg, 0);
                        HandleLabelRef();
                        return true;
                    }
                }
            }
            return false;
        }
        case kFloatImm: {
            if (Next() == kRegister) {
                auto reg = lexer_.reg_val();
                if (Next() == ',') {
                    Next();
                    if (tok_type == kRegister) {
                        GenRegReg(reg, lexer_.reg_val());
                        return true;
                    }
                    else if (tok_type == kFloat) {
                        InstFloatImm inst = {index, (unsigned char)(reg << 4), lexer_.float_val()};
                        WriteBytes(out_, inst);
                        return true;
                    }
                }
            }
            return false;
        }
        case kReg: {
            if (Next() == kRegister) {
                GenRegReg(lexer_.reg_val(), 0);
                return true;
            }
            return false;
        }
        case kRegReg: {
            if (Next() == kRegister) {
                auto reg = lexer_.reg_val();
                if (Next() == kRegister) {
                    GenRegReg(reg, lexer_.reg_val());
                    return true;
                }
            }
            return false;
        }
        case kVoid: {
            InstVoid inst = {index};
            WriteBytes(out_, inst);
            return true;
        }
        case kRegInt: {
            Next();
            if (tok_type == kRegister) {
                GenRegReg(lexer_.reg_val(), 1);
                return true;
            }
            else if (tok_type == kNumber) {
                InstIntImm inst = {index, 0, lexer_.num_val()};
                WriteBytes(out_, inst);
                return true;
            }
            else if (tok_type == kLabelRef) {
                InstIntImm inst = {index, 0, 0};
                WriteBytes(out_, inst);
                out_.seekp(-sizeof(unsigned int), std::ios_base::cur);
                HandleLabelRef();
                return true;
            }
            return false;
        }
        case kCall: {
            Next();
            if (tok_type == kRegister) {
                GenRegReg(lexer_.reg_val(), 1);
                return true;
            }
            else if (tok_type == kNumber) {
                auto count = lexer_.num_val();
                if (Next() == ',' && Next() == kNumber) {
                    auto stack = lexer_.num_val();
                    if (Next() == ',') {
                        Next();
                        Function func = {0, (unsigned char)count, (unsigned short)stack, 0};
                        GenRegReg(0, 0);
                        if (tok_type == kNumber) {
                            func.position = lexer_.num_val();
                            WriteBytes(out_, func);
                            return true;
                        }
                        else if (tok_type == kLabelRef) {
                            WriteBytes(out_, func);
                            out_.seekp(-sizeof(unsigned int), std::ios_base::cur);
                            HandleLabelRef();
                            return true;
                        }
                    }
                }
            }
            return false;
        }
        case kST: {
            if (Next() == kNumber) { // TODO
                auto dst = lexer_.num_val();
                if (Next() == ',') {
                    Next();
                    if (tok_type == kRegister) {
                        InstIntImm inst = {index, (unsigned char)((lexer_.reg_val() << 4) + 1), dst};
                        WriteBytes(out_, inst);
                        return true;
                    }
                    else if (tok_type == kNumber) {
                        InstDoubleInt inst = {index, 0, dst, lexer_.num_val()};
                        WriteBytes(out_, inst);
                        return true;
                    }
                }
            }
            return false;
        }
        case kINT: {
            if (Next() == kNumber) {
                InstIntOnly inst = {index, lexer_.num_val()};
                WriteBytes(out_, inst);
                return true;
            }
            return false;
        }
        case kMOVL: {
            if (Next() == kRegister && Next() == ',' && Next() == kNumber) {
                auto count = lexer_.num_val();
                if (Next() == ',') {
                    Next();
                    List lst = {(unsigned int)(count * sizeof(List)), 0};
                    GenRegReg(lexer_.reg_val(), 0);
                    if (tok_type == kNumber) {
                        lst.position = lexer_.num_val();
                        WriteBytes(out_, lst);
                        return true;
                    }
                    else if (tok_type == kLabelRef) {
                        WriteBytes(out_, lst);
                        out_.seekp(-sizeof(unsigned int), std::ios_base::cur);
                        HandleLabelRef();
                        return true;
                    }
                }
            }
            return false;
        }
        default: {
            return false;
        }
    }
}

int Generator::Generate() {
    int tok_type;
    auto Next = [&]() { return (tok_type = lexer_.NextToken()); };

    for(;;) {
        Next();
        switch_tok:;

        auto cur_pos = out_.tellp();
        if (cur_pos > 0xFFFFFFFF) {
            PrintError("file size too big");
            return error_num_;
        }

        switch (tok_type) {
            case kLabelDef: {
                if (lab_list.count(lexer_.lab_val()) != 0) {
                    PrintError("conflicted label defination");
                }
                else {
                    unsigned int jmp_pos = (unsigned int)cur_pos;
                    if (lexer_.lab_val().substr(0, 2) == "__") {
                        section_tag = lexer_.lab_val().substr(2);
                    }
                    else {
                        if (section_tag == "CONSTANT") {
                            jmp_pos = jmp_pos - lab_list["__CONSTANT"] + arg_stack_size;
                        }
                        else if (section_tag == "PROGRAM") {
                            jmp_pos = jmp_pos - lab_list["__PROGRAM"];
                        }
                    }
                    lab_list[lexer_.lab_val()] = jmp_pos;
                    auto found = false;
                    for (const auto &i : lab_fill) {
                        if (i.first == lexer_.lab_val()) {
                            out_.seekp(i.second);
                            unsigned int temp = (unsigned int)jmp_pos;
                            WriteBytes(out_, temp);
                            out_.seekp(cur_pos);
                            found = true;
                        }
                    }
                    if (found) lab_fill.erase(lexer_.lab_val());
                }
                break;
            }
            case kOperator: {
                if (lexer_.op_val() == DEF) {
                    do {
                        switch (Next()) {
                            case kNumber: {
                                auto num = lexer_.num_val();
                                if (out_.tellp() == 9) arg_stack_size = num;
                                WriteBytes(out_, num);
                                break;
                            }
                            case kFloat: {
                                auto fp = lexer_.float_val();
                                WriteBytes(out_, fp);
                                break;
                            }
                            case kChar: {
                                out_ << lexer_.char_val();
                                break;
                            }
                            case kString: {
                                out_ << lexer_.str_val();
                                out_ << '\0';
                                break;
                            }
                            case kLabelRef: {
                                HandleLabelRef();
                                break;
                            }
                            default: {
                                PrintError("invalid defination");
                                break;
                            }
                        }
                    } while (Next() == ',');
                    goto switch_tok;
                }
                else if (!HandleOperator()) {
                    PrintError("invalid operator");
                    goto switch_tok;
                }
                break;
            }
            case kEOF: {
                if (!lab_fill.empty()) {
                    for (const auto &i : lab_fill) {
                        std::fprintf(stderr, "position: %u \033[31m\033[1merror:\033[0m undefined label \"%s\"\n", (unsigned int)i.second, i.first.c_str());
                        ++error_num_;
                    }
                }
                return error_num_;
            }
            case kError: default: {
                PrintError("unkonwn token");
                break;
            }
        }
    }
}
