#include <utility>
#include <algorithm> 
#include <vector>
#include "state.hpp"
#include "alphabeta.hpp"

/*============================================================
 * AlphaBeta — eval_ctx
 *============================================================*/
int AlphaBeta::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    if(state->game_state == WIN){
        return P_MAX - ply;
    }

    if(state->game_state == DRAW){
        return 0;
    }

    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }
    
    history.push(state->hash());

    if(depth <= 0){
        int score = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history); 
        history.pop(state->hash());
        return score;
    }

    struct ScoredAction {
        Move action;
        int w;
    };

    std::vector<ScoredAction> scored_actions;
    scored_actions.reserve(state->legal_actions.size());

    for(const auto& action : state->legal_actions){
        int w = 0;
        int r = action.second.first; //終點座標x
        int c = action.second.second;//終點座標y
        if(state->board.board[0][r][c] != 0 || state->board.board[1][r][c] != 0){
            w += 10;
        }
        scored_actions.push_back({action, w});
    }

    std::sort(
                scored_actions.begin(),
                scored_actions.end(),
                [](const ScoredAction& a,const ScoredAction& b){
                    return a.w > b.w;
                }
            );
    
    int i; 
    for(i=0;i < state->legal_actions.size();++i){
        state->legal_actions[i] = scored_actions[i].action;
    }

    int best_score = M_MAX;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int raw;
        if(same){
            raw = eval_ctx(next, depth - 1, alpha, beta, history, ply + 1, ctx, p);
        }else{
            raw = eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
        }

        int score = same ? raw : -raw;
        delete next;

        if(score > best_score){
            best_score = score;
        }
        if(best_score > alpha){
            alpha = best_score;
        }
        if(alpha >= beta){
            break;
        }
    }

    history.pop(state->hash());
    return best_score;
}


/*============================================================
 * AlphaBeta — search
 *============================================================*/
SearchResult AlphaBeta::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

    if(!state->legal_actions.empty()){
        result.best_move = state->legal_actions[0];
    }

    struct RootMove{
        Move action;
        int score;
    };

    std::vector<RootMove> root_moves;
    root_moves.reserve(state->legal_actions.size());

    int alpha = M_MAX - 10;
    int beta = P_MAX;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int raw;
        if(same){
            raw = eval_ctx(
                next,
                depth - 1,
                alpha,
                beta,
                history,
                1,
                ctx,
                p
            );
        }else{
            raw = eval_ctx(
                next,
                depth - 1,
                -beta,
                -alpha,
                history,
                1,
                ctx,
                p
            );
        }
        int score = same ? raw : -raw;
        delete next;

        if(score > alpha){
            alpha = score;
            result.best_move = action;

            if(p.report_partial && ctx.on_root_update){
               ctx.on_root_update({result.best_move, alpha, depth, move_index + 1, total_moves});
            }
        }  
        move_index++;
    }

    result.score = alpha;
    result.seldepth = ctx.seldepth;
    result.nodes = ctx.nodes;

    return result;
}


/*============================================================
* AlphaBeta — default_params / param_defs
 *============================================================*/
ParamMap AlphaBeta::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> AlphaBeta::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
