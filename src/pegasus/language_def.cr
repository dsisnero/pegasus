require "./elements.cr"
require "./items.cr"
require "./grammar.cr"
require "./nfa.cr"
require "./regex.cr"
require "./nfa_to_dfa.cr"
require "./table.cr"
require "./error.cr"
require "./generated/grammar_parser.cr"

module Pegasus
  module Language
    class NamedConflictErrorContext < Pegasus::Error::ErrorContext
      def initialize(@nonterminals : Array(String))
      end

      def to_s(io)
        io << "The nonterminals involved are: "
        @nonterminals.join(", ", io)
      end
    end

    # The complete data class, built to be all the information
    # needed to construct a parser generator.
    class LanguageData
      # Table for tokens that should be skipped.
      getter lex_skip_table : Array(Bool)
      # The state table for the lexer, which is used for transitions
      # of the `Pegasus::Nfa::Nfa` during tokenizing.
      getter lex_state_table : Array(Array(Int64))
      # The table that maps a state ID to a token ID, used to
      # recognize that a match has occured.
      getter lex_final_table : Array(Int64)
      # Transition table for the LALR parser automaton, indexed
      # by terminal and nonterminal IDs.
      getter parse_state_table : Array(Array(Int64))
      # Action table indexed by the state and the lookahead item.
      # Used to determine what the parser should do in each state.
      getter parse_action_table : Array(Array(Int64))

      # The terminals, and their original names / regular expressions.
      getter terminals : Hash(String, Pegasus::Pda::Terminal)
      # The nonterminals, and their original names.
      getter nonterminals : Hash(String, Pegasus::Pda::Nonterminal)
      # The items in the language. Used for reducing / building up
      # trees once a reduce action is performed.
      getter items : Array(Pegasus::Pda::Item)
      # The highest terminal ID, used for correctly accessing the
      # tables indexed by both terminal and nonterminal IDs.
      getter max_terminal : Int64

      # Creates a new language data object.
      def initialize(language_definition)
        @terminals, @nonterminals, grammar =
          generate_grammar(language_definition)
        @lex_skip_table, @lex_state_table, @lex_final_table,
          @parse_state_table, @parse_action_table =
          generate_tables(language_definition, @terminals, @nonterminals, grammar)
        @max_terminal = @terminals.values.max_of?(&.id) || 0_i64
        @items = grammar.items
      end

      # Assigns an ID to each unique vaue in the iterable.
      private def assign_ids(values : Iterable(T), &block : Int64 -> R) forall T, R
        hash = {} of T => R
        last_id = 0_i64
        values.each do |value|
          next if hash[value]?
          hash[value] = yield (last_id += 1) - 1
        end
        return hash
      end

      # Creates a grammar, returning it and the hashes with identifiers for
      # the terminals and nonterminals.
      private def generate_grammar(language_def)
        token_ids = assign_ids(language_def.tokens.keys) do |i|
          Pegasus::Pda::Terminal.new i
        end
        rule_ids = assign_ids(language_def.rules.keys) do |i|
          Pegasus::Pda::Nonterminal.new i
        end

        grammar = Pegasus::Pda::Grammar.new token_ids.values, rule_ids.values
        language_def.rules.each do |name, bodies|
          head = rule_ids[name]
          bodies.each &.alternatives.each do |body|
            body = body.elements.map do |element_name|
              element = token_ids[element_name]? || rule_ids[element_name]?
              raise_grammar "No terminal or rule named #{element_name}" unless element
              next element
            end
            item = Pegasus::Pda::Item.new head, body
            grammar.add_item item
          end
        end

        return { token_ids, rule_ids, grammar }
      end

      # Generates lookup tables using the given terminals, nonterminals,
      # and grammar.
      private def generate_tables(language_def, terminals, nonterminals, grammar)
        nfa = Pegasus::Nfa::Nfa.new
        terminals.each do |terminal, value|
          nfa.add_regex language_def.tokens[terminal].regex, value.id
        end
        dfa = nfa.dfa

        begin
          lex_skip_table = [ false ] +
            language_def.tokens.map &.[1].options.includes?("skip")
          lex_state_table = dfa.state_table
          lex_final_table = dfa.final_table

          lr_pda = grammar.create_lr_pda(nonterminals.values.find { |it| it.id == 0 })
          lalr_pda = grammar.create_lalr_pda(lr_pda)
          parse_state_table = lalr_pda.state_table
          parse_action_table = lalr_pda.action_table
        rescue e : Pegasus::Error::PegasusException
          if old_context = e.context_data
            .find(&.is_a?(Pegasus::Dfa::ConflictErrorContext))
            .as?(Pegasus::Dfa::ConflictErrorContext)

            names = old_context.item_ids.map do |id|
              head = grammar.items[id].head
              nonterminals.key_for head
            end
            e.context_data.delete old_context
            e.context_data << NamedConflictErrorContext.new names
          end
          raise e
        end

        return { lex_skip_table, lex_state_table, lex_final_table, parse_state_table, parse_action_table }
      end
    end

    class Pegasus::Generated::Tree
      alias SelfDeque = Deque(Pegasus::Generated::Tree)

      protected def flatten_recursive(*, value_index : Int32, recursive_name : String, recursive_index : Int32) : SelfDeque
        if flattened = self.as?(Pegasus::Generated::NonterminalTree)
          recursive_child = flattened.children[recursive_index]?
          value_child = flattened.children[value_index]?

          if flattened.name == recursive_name && recursive_child
            add_to = recursive_child.flatten_recursive(
              value_index: value_index,
              recursive_name: recursive_name,
              recursive_index: recursive_index)
          else
            add_to = SelfDeque.new
          end
          add_to.insert(0, value_child) if value_child

          return add_to
        end
        return SelfDeque.new
      end

      # Since currently, * and + operators aren't supported in Pegasus grammars, they tend to be recursively written.
      # This is a utility function to "flatten" a parse tree produced by a recursively written grammar.
      def flatten(*, value_index : Int32, recursive_name : String, recursive_index : Int32)
        flatten_recursive(
          value_index: value_index,
          recursive_name: recursive_name,
          recursive_index: recursive_index).to_a
      end
    end

    alias Option = String
    
    # Since Pegasus supports options on tokens and rules,
    # we need to represent an object to which options can be attached.
    # this is this type of object.
    abstract class OptionObject
      # Gets the actual list of options attached to this object.
      getter options : Array(Option)

      def initialize
        @options = [] of Option
      end
    end

    # A token declaration, with zero or more rules attached to it.
    class Token < OptionObject
      # Gets the regular expression that defines this token.
      getter regex : String

      def initialize(@regex, @options = [] of Option)
      end

      def ==(other : Token)
        return (other.regex == @regex) && (other.options == @options)
      end

      def hash(hasher)
        @regex.hash(hasher)
        @options.hash(hasher)
        hasher
      end
    end

    # An element of a grammar rule. Can be either a token or another rule.
    class RuleElement
      # The name of the element, as specified in the grammar.
      getter name : String

      def initialize(@name)
      end

      def ==(other : RuleElement)
        return @name == other.name
      end
    end

    # An element that is optional.
    class OptionalElement < RuleElement
    end

    # An element that is repeated one or more times.
    class OneOrMoreElement < RuleElement
    end

    # An element that is repeated zero or more times.
    class ZeroOrMoreElement < RuleElement
    end

    # One of the alternatives of a rule. 
    class RuleAlternative
      # The elements of the rule.
      getter elements : Array(RuleElement)

      def initialize(@elements)
      end

      def ==(other : RuleAlternative)
        return @elements == other.elements
      end
    end

    # A single rule. This can have one or more alternatives,
    # but has the same rules (zero or more) applied to them.
    class Rule < OptionObject
      getter alternatives : Array(RuleAlternative)

      def initialize(@alternatives, @options = [] of Option)
      end

      def ==(other : Rule)
        return (other.alternatives == @alternatives) && (other.options == @options)
      end

      def hash(hasher)
        @alternatives.hash(hasher)
        @options.hash(hasher)
        hasher
      end
    end

    # A language definition parsed from a grammar string.
    class LanguageDefinition
      getter tokens : Hash(String, Token)
      getter rules : Hash(String, Array(Rule))

      # Creates a new, empty language definition.
      def initialize
        @tokens = {} of String => Token
        @rules = {} of String => Array(Rule)
      end

      # Creates a new language definition from the given string.
      def initialize(s : String)
        @tokens = {} of String => Token
        @rules = {} of String => Array(Rule)
        from_string(s)
      end

      # Creates a new language definition from the given IO.
      def initialize(io : IO)
        @tokens = {} of String => Token
        @rules = {} of String => Array(Rule)
        from_io(io)
      end

      private def extract_options(statement_end_tree)
        statement_end_tree = statement_end_tree.as(Pegasus::Generated::NonterminalTree)
        return [] of Option unless statement_end_tree.children.size > 1
        options_tree = statement_end_tree.children[0].as(Pegasus::Generated::NonterminalTree)
        options = options_tree.children[1]
          .flatten(value_index: 0, recursive_name: "option_list", recursive_index: 2)
          .map(&.as(Pegasus::Generated::NonterminalTree).children[0])
          .map(&.as(Pegasus::Generated::TerminalTree).string)
      end

      private def extract_tokens(token_list_tree)
        token_list_tree.flatten(value_index: 0, recursive_name: "token_list", recursive_index: 1)
          .map { |it| ntt = it.as(Pegasus::Generated::NonterminalTree); { ntt.children[1], ntt.children[3], ntt.children[4] } }
          .map do |data|
            name_tree, regex_tree, statement_end = data
            name = name_tree
              .as(Pegasus::Generated::TerminalTree).string
            raise_grammar "Declaring a token (#{name}) a second time" if @tokens.has_key? name
            regex = regex_tree
              .as(Pegasus::Generated::TerminalTree).string[1..-2]
            @tokens[name] = Token.new regex, extract_options(statement_end)
          end
      end

      private def extract_bodies(bodies_tree)
        bodies_tree.flatten(value_index: 0, recursive_name: "grammar_bodies", recursive_index: 2)
          .map do |body|
            RuleAlternative.new body
              .flatten(value_index: 0, recursive_name: "grammar_body", recursive_index: 1)
              .map(&.as(Pegasus::Generated::TerminalTree).string)
              .map { |it| RuleElement.new it }
        end
      end

      private def extract_rules(grammar_list_tree)
        grammar_list_tree.flatten(value_index: 0, recursive_name: "grammar_list", recursive_index: 1)
          .map { |it| ntt = it.as(Pegasus::Generated::NonterminalTree); { ntt.children[1], ntt.children[3], ntt.children[4] } }
          .map do |data|
            name_tree, bodies_tree, statement_end = data
            name = name_tree
              .as(Pegasus::Generated::TerminalTree).string
            raise_grammar "Declaring a rule (#{name}) with the same name as a token" if @tokens.has_key? name
            bodies = extract_bodies(bodies_tree)

            unless old_rules = @rules[name]?
              @rules[name] = old_rules = Array(Rule).new
            end
            old_rules << Rule.new bodies, extract_options(statement_end)
          end
      end

      # Creates a language definition from a string.
      private def from_string(string)
        tree = Pegasus::Generated.process(string).as(Pegasus::Generated::NonterminalTree)
        if tokens = tree.children.find &.as(Pegasus::Generated::NonterminalTree).name.==("token_list")
          extract_tokens(tokens)
        end
        if rules = tree.children.find &.as(Pegasus::Generated::NonterminalTree).name.==("grammar_list")
          extract_rules(rules)
        end
      rescue e : Pegasus::Error::PegasusException
        raise e
      rescue e : Exception
        raise_grammar e.message.not_nil!
      end

      # Creates a languge definition from IO.
      private def from_io(io)
        string = io.gets_to_end
        from_string(string)
      end
    end
  end
end
