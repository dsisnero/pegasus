require "./nfa.cr"

module Pegasus
  module Nfa
    class Transition
      def label
        ""
      end
    end

    class CharTransition
      def label
        "#{@char}"
      end
    end

    class LambdaTransition
      def label
        "(lambda)"
      end
    end

    class State
      def label
        "q#{@id}"
      end
    end

    class Nfa
      def to_dot
        memory = IO::Memory.new
        memory << "digraph G {\n"
        if s = @start
          memory << "  "
          memory << s.label
          memory << " [shape=diamond]\n"
        end
        @states.each do |state|
          label = "q#{state.id}"
          if state.final
            memory << "  "
            memory << label
            memory << " [shape=doublecircle]\n"
          end
          state.transitions.each do |t|
            other_label = t.other.label
            t_label = t.label

            memory << "  "
            memory << state.label
            memory << " -> "
            memory << other_label
            memory << " [ label=\""
            memory << t_label
            memory << "\" ] \n"
          end
        end
        memory << "}"
        return memory.to_s
      end
    end
  end
end
